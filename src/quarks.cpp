#include <quarks.hpp>
#include <v8engine.hpp>

#include <sorter.hpp>

#include "rocksdb/db.h"

#include <iomanip>

#include <iostream>
#include <boost/asio.hpp>

#include <algorithm>

using namespace boost::asio;
using ip::tcp;
//using std::string;
//using std::cout;
//using std::endl;

using namespace Quarks;

Core Core::_Instance;
Matrix Matrix::_Instance;

rocksdb::DB* db = nullptr;
rocksdb::Status dbStatus;

void openDB(std::string schemaname){
    rocksdb::Options options;
    options.create_if_missing = true;
    dbStatus = rocksdb::DB::Open(options, schemaname, &db);
    CROW_LOG_INFO << "opening schema: " << schemaname;

}

void closeDB(std::string schemaname){
   
    CROW_LOG_INFO << "closing schema: " << schemaname;
    
    // close the database
    delete db; // deleting causes segmentation fault, let the memory leak take over :(
    
}

int wildcmp(const char *wild, const char *str) {
    // Written by Jack Handy - <A href="mailto:jakkhandy@hotmail.com">jakkhandy@hotmail.com</A>
    const char *cp = NULL, *mp = NULL;
    
    while ((*str) && (*wild != '*')) {
        if ((*wild != *str) && (*wild != '?')) {
            return 0;
        }
        wild++;
        str++;
    }
    
    while (*str) {
        if (*wild == '*') {
            if (!*++wild) {
                return 1;
            }
            mp = wild;
            cp = str+1;
        } else if ((*wild == *str) || (*wild == '?')) {
            wild++;
            str++;
        } else {
            wild = mp;
            str = cp++;
        }
    }
    
    while (*wild == '*') {
        wild++;
    }
    return !*wild;
}

Core::Core() : _portNumber(18080){
    
}

Core::~Core(){
    
}

void Core::setEnvironment(int argc, char** argv){
    
    std::vector<std::string> arguments(argv + 1, argv + argc);
    _argv = std::move(arguments);
    _argc = argc;
    
    std::string schemaname = "quarks_db";
    
    bool schema = false;
    bool port = false;
    
    for(auto v : _argv){
        CROW_LOG_INFO << " v = " << v << " ";
        if(schema){
            schemaname = v;
            schema = false;
        }else if(port){
            _portNumber = std::stoi(v);
            port = false;
        }
        
        if(!v.compare("-schema")){
            schema = true;
        }else if(!v.compare("-port")){
            port = true;
        }
    }
    
    openDB(schemaname);
    _argv.push_back(schemaname);    
    
}

void Core::shutDown(){
     closeDB(_argv.back());
}

int Core::getPort(){
    return _portNumber;
}

bool getKeyValuePair(std::string key, std::string& value, std::string& out){
    bool ret = false;
    
    if (dbStatus.ok()){
        rocksdb::ReadOptions ro;
        rocksdb::Iterator* it = db->NewIterator(ro);
        
        try{
            rocksdb::Slice prefix(key);
            //rocksdb::Slice prefixPrint = prefix;
            //CROW_LOG_INFO << "prefix : " << prefixPrint.ToString();
            
            it->Seek(prefix);
            if(it->Valid() && !key.compare(it->key().ToString())){
                ret = true;
                value = it->value().ToString();
                out = R"({"result":true})";
                
            }else{
                out = R"({"result":false})";
            }
            
        }catch (const std::runtime_error& error){
            CROW_LOG_INFO << "Runtime Error: " <<  it->value().ToString();
            
            out = R"({"error":"runtime error"})";
            
        }
        
        delete it;
        
    }else{
        out = R"({"error":"db status error"})";
    }
    
    return ret;
}

bool insertKeyValuePair(bool failIfExists, crow::json::rvalue& x, std::string& out){
    
    std::string key = x["key"].s();
    CROW_LOG_INFO << "key found :  " << key;
    
    std::string result = "true";
    std::string error = "";
    
    std::string readValue;
    bool foundKey = getKeyValuePair(key, readValue, out);
    if(failIfExists){
        if(foundKey){
            result = "false";
            error = ",\"error\":\"key already exists\"";
            
            out = std::string("{") + R"("result":)" + result + error + std::string("}");
            
            return false;
        }
    }
    
    crow::json::wvalue w;
    crow::json::wvalue putValue = x["value"];
    
    bool moveValueJson = true;
    if(foundKey){
        crow::json::wvalue existingValues = crow::json::load(readValue);
        
        std::vector<std::string> putValueKeys = putValue.keys();
        
        if(putValueKeys.size() > 0){
            moveValueJson = false;
            
            std::vector<std::string> existingKeys = existingValues.keys();
            
            for(auto e : existingKeys){
                w[e] = std::move(existingValues[e]);
            }
            for(auto p : putValueKeys){
                w[p] = std::move(putValue[p]);
            }
            
        }
        
    }
    
    if(moveValueJson){
        w = std::move(putValue);
    }
    
    std::string writeValue = crow::json::dump(w);
    
    CROW_LOG_INFO << "put body : " << "key : " << key << ", value >> " << writeValue << "\n";
    
    bool ret = false;
    
    rocksdb::Slice keySlice = key;
    
    // modify the database
    if (dbStatus.ok()){
        ret = true;
        
        rocksdb::Status status = db->Put(rocksdb::WriteOptions(), keySlice, writeValue);
        if(!status.ok()){
            //result = "{\"error\":\"data failed to save\"}";
            result = "false";
            error = ",\"error\":\"data failed to save\"";
            ret = false;
        }
        
    }else{
        //result = "{\"error\":\"db status error\"}";
        result = "false";
        error = ",\"error\":\"db status error\"";
        ret = false;
    }
    
    //std::stringstream ss;
    //ss<< "{" << std::quoted("result") << ":" << std::quoted(key) << "}";
    if(ret){
        out = std::string("{") + R"("result":)" + result + std::string("}");
    }else{
        out = std::string("{") + R"("result":)" + result + error + std::string("}");
    }
    
    return  ret;
}

bool Core::insert(bool failIfExists, std::string body, std::string& out){
    auto x = crow::json::load(body);
    
    if (!x){
        CROW_LOG_INFO << "invalid put body" << body;
        //out = "invalid put body";
        
        out = "{\"error\": \"Invalid put body\"}";
        
        return false;
        
    }
    
    return insertKeyValuePair(failIfExists, x, out);
}

bool Core::post(std::string body, std::string& out) {
    return insert(true, body, out);
   
}

bool Core::put(std::string body, std::string& out) {
    return insert(false, body, out);
    
}

bool Core::putAtom(crow::json::rvalue& x, std::string& out){
    out = std::string("{") + R"("result":false)" + std::string("}");
    
    bool ret = true;
    
    for(auto& v : x){
        CROW_LOG_INFO << ".. put: " << crow::json::dump(v);
        ret &= insertKeyValuePair(false, v, out);
        if(!ret){
            break;
        }
    }
    
    return ret;
}

bool Core::putAtom(std::string body, std::string& out) {
    auto x = crow::json::load(body);
    CROW_LOG_INFO << "put body" << body;
    if (!x){
        CROW_LOG_INFO << "invalid put body" << body;
        //out = "invalid put body";
        
        out = "{\"error\": \"Invalid put body\"}";
        
        return false;
        
    }
    
    return putAtom(x, out);
    
}


bool Core::exists(std::string key, std::string& out){
    std::string value;
    return getKeyValuePair(key, value, out);
}

bool Core::get(std::string key, std::string& value){
    
    bool ret = false;
    
    if (dbStatus.ok()){
        rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &value);
        
        if(status.ok()){
            ret = true;
        }
    }
    
    return ret;
    
}

bool Core::getAll(std::string wild,
                  std::vector<crow::json::wvalue>& matchedResults,
                  int skip /*= 0*/, int limit /*= -1*/) {
    
    bool ret = true;
    if (dbStatus.ok()){
        
        std::size_t found = wild.find("*");
        if(found != std::string::npos && found == 0){
            return false;
        }
        // create new iterator
        rocksdb::ReadOptions ro;
        rocksdb::Iterator* it = db->NewIterator(ro);
        
        std::string pre = wild.substr(0, found);
        
        rocksdb::Slice prefix(pre);
        
        rocksdb::Slice prefixPrint = prefix;
        CROW_LOG_INFO << "prefix : " << prefixPrint.ToString();
        
        
        int i  = -1;
        int count = (limit == -1) ? INT_MAX : limit;
        
        int lowerbound  = skip - 1;
        int upperbound = skip + count;

        for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
            
            if(wildcmp(wild.c_str(), it->key().ToString().c_str())){
                crow::json::wvalue w;
                
                try{
                    
                    i++;
                    
                    if(i > lowerbound && (i < upperbound || limit == -1)){
                        auto x = crow::json::load(it->value().ToString());
                        
                        if(!x){
                            w =  crow::json::load(std::string("[\"") +
                                                  it->value().ToString() + std::string("\"]"));
                            
                        }else{
                            w = x;
                        }
                        
                        //CROW_LOG_INFO << "w fwd: " << crow::json::dump(w) << " skip: "
                        //<< skip << ", limit: " << limit;
                        
						matchedResults.push_back(std::move(w));
                        
                    }
                    
                    if((i == upperbound) && (limit != -1)){
                        break;
                    }
                    
                }catch (const std::runtime_error& error){
                    CROW_LOG_INFO << "Runtime Error: " << i << it->value().ToString();
                    
                    w["error"] =  it->value().ToString();
                    
                    matchedResults.push_back(std::move(w));
                    
                    ret = false;
                }
                
            }
        }
        
        // do something after loop
        
        delete it;
        
    }
    
    return ret && dbStatus.ok();
}

bool Core::getSorted(std::string wild, std::string sortby, bool ascending,
                   std::vector<crow::json::wvalue>& matchedResults,
                   int skip /*= 0*/, int limit /*= -1*/) {
    
    bool ret = true;
    
    if (dbStatus.ok()){
        
        std::size_t found = wild.find("*");
        if(found != std::string::npos && found == 0){
            return false;
        }
        
        // create new iterator
        rocksdb::ReadOptions ro;
        rocksdb::Iterator* it = db->NewIterator(ro);
        
        std::string pre = wild.substr(0, found);
        
        rocksdb::Slice prefix(pre);
        
        rocksdb::Slice prefixPrint = prefix;
        CROW_LOG_INFO << "prefix : " << prefixPrint.ToString();
        
        
        int i  = -1;
        int count = (limit == -1) ? INT_MAX : limit;
        
        int lowerbound  = skip - 1;
        int upperbound = skip + count;
        
        //bool isNumber = false;
        bool isSortable = sortby.size() > 0;
        
        
        std::vector<crow::json::rvalue> allResults;
               
        for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
            
            if(wildcmp(wild.c_str(), it->key().ToString().c_str())){                
                
                //crow::json::wvalue w;                
                
                try{
                	auto x = crow::json::load(it->value().ToString());
                
                    if(!x){
                        isSortable = false;
                    }else{
						
                        try{
                            if(x.t() == crow::json::type::Object){
                                i++;
                                //CROW_LOG_INFO << "obj to sort: " << i << ". " << crow::json::dump(x);
                                //w["value"] = x;
                                //w["key"] = it->key().ToString();
                                allResults.push_back(std::move(x));
                            }
	                        
                    	} catch (const std::runtime_error& error){
                        	CROW_LOG_INFO << "Runtime Sort Error 1: " << error.what();
                        	isSortable = false;
                        
                        	ret = false;
                        
                    	};
                    
                    }                    
                	
                } catch (const std::runtime_error& error){
                    CROW_LOG_INFO << "Runtime Sort Error 2: " << error.what() << " , " << i << " . " << it->value().ToString();
                    //w["error"] =  it->value().ToString();
                    isSortable = false;
                    
                    //matchedResults.push_back(std::move(w));
                    
                    ret = false;
                }
              
            }
        }        
               
        // do something after loop
        
		if(isSortable){
            //CROW_LOG_INFO << "sorter: " << sortby;
            //Sorter::JsonComparer c(sortby, ascending);
            try{
                std::sort(allResults.begin(), allResults.end(), Sorter::JsonComparer(sortby));
                
            } catch (const std::runtime_error& error){
                CROW_LOG_INFO << "Runtime Sort Error 3: Not Sortable";
                ret = false;
            }
			
		}
		
        
		i = -1;
        if(!ascending){
            for(auto& x : Sorter::backwards< std::vector<crow::json::rvalue> >(allResults)){
                i++;
                if(i > lowerbound && (i < upperbound || limit == -1)){
                    try{
                        crow::json::wvalue w = x;
                        CROW_LOG_INFO << "sorted object: " << i << ". " << crow::json::dump(w);
                        matchedResults.push_back(std::move(w));
                        
                    }catch (const std::runtime_error& error){
                        CROW_LOG_INFO << "Runtime Sort Error 4.1: " << error.what() << " , " << i;
                    }
                }
                
                if((i == upperbound) && (limit != -1)){
                    break;
                }
            }
        }else{
            for(auto& x : allResults){
                i++;
                if(i > lowerbound && (i < upperbound || limit == -1)){
                    try{
                        crow::json::wvalue w = x;
                        CROW_LOG_INFO << "sorted object: " << i << ". " << crow::json::dump(w);
                        matchedResults.push_back(std::move(w));
                        
                    }catch (const std::runtime_error& error){
                        CROW_LOG_INFO << "Runtime Sort Error 4.2: " << error.what() << " , " << i;
                    }
                }
                
                if((i == upperbound) && (limit != -1)){
                    break;
                }
            }
            
        }
		
                
        delete it;
        
    }
    
    return ret && dbStatus.ok();
}

bool Core::getKeys(std::string wild,
                  std::vector<crow::json::wvalue>& matchedResults,
                  int skip /*= 0*/, int limit /*= -1*/) {
    
    bool ret = true;
    if (dbStatus.ok()){
        
        std::size_t found = wild.find("*");
        if(found != std::string::npos && found == 0){
            return false;
        }

        // create new iterator
        rocksdb::ReadOptions ro;
        rocksdb::Iterator* it = db->NewIterator(ro);
        
        std::string pre = wild.substr(0, found);
        
        rocksdb::Slice prefix(pre);
        
        rocksdb::Slice prefixPrint = prefix;
        CROW_LOG_INFO << "prefix : " << prefixPrint.ToString();
        
        
        int i  = -1;
        int count = (limit == -1) ? INT_MAX : limit;
        
        int lowerbound  = skip - 1;
        int upperbound = skip + count;
        
        for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
            
            if(wildcmp(wild.c_str(), it->key().ToString().c_str())){
                
                i++;
                
                if(i > lowerbound && (i < upperbound || limit == -1)){
                    crow::json::wvalue w;
                    
                    try{
                    	auto x = crow::json::load(it->value().ToString());
                    
                        if(!x){
                            w["value"] =  crow::json::load(std::string("[\"") +
                                                  it->value().ToString() + std::string("\"]"));
                            
                        }else{
                            w["value"] = x;
                        }
                        
                    }catch (const std::runtime_error& error){
                        CROW_LOG_INFO << "Runtime Error: " << i << it->value().ToString();
                        
                        w["error"] =  it->value().ToString();
                        
                        matchedResults.push_back(std::move(w));
                        
                        ret = false;
                    }
                    
                    w["key"] = it->key().ToString();
                    
                    //CROW_LOG_INFO << "w key fwd: " << crow::json::dump(w) << " skip: "
                    //<< skip << ", limit: " << limit;
                    
					matchedResults.push_back(std::move(w));
                    
                }
                
                if((i == upperbound) && (limit != -1)){
                    break;
                }
                
            }
        }
        
        
        
        // do something after loop
        
        delete it;
        
    }
    
    return ret && dbStatus.ok();
}

bool Core::getKeysReversed(std::string wild,
                   std::vector<crow::json::wvalue>& matchedResults,
                   int skip /*= 0*/, int limit /*= -1*/) {
    
    bool ret = true;
    if (dbStatus.ok()){
        
        std::size_t found = wild.find("*");
        if(found != std::string::npos && found == 0){
            return false;
        }
        
        // create new iterator
        rocksdb::ReadOptions ro;
        rocksdb::Iterator* it = db->NewIterator(ro);
        
        std::string pre = wild.substr(0, found);
        
        rocksdb::Slice prefix(pre);
        
        rocksdb::Slice prefixPrint = prefix;
        CROW_LOG_INFO << "prefix : " << prefixPrint.ToString();
        
        
        int i  = -1;
        int count = (limit == -1) ? INT_MAX : limit;
        
        int lowerbound  = skip - 1;
        int upperbound = skip + count;
        
        std::vector<crow::json::wvalue> allResults;
        
        for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
            
            if(wildcmp(wild.c_str(), it->key().ToString().c_str())){
                
                crow::json::wvalue w;
                try{
                    auto x = crow::json::load(it->value().ToString());
                    
                    if(!x){
                        w["value"] =  crow::json::load(std::string("[\"") +
                                                       it->value().ToString() + std::string("\"]"));
                        
                    }else{
                        w["value"] = x;
                    }
                    
                }catch (const std::runtime_error& error){
                    CROW_LOG_INFO << "Runtime Error: " << i << it->value().ToString();
                    
                    w["error"] =  it->value().ToString();
                    
                    allResults.push_back(std::move(w));
                    
                    ret = false;
                }
                
                w["key"] = it->key().ToString();
                
                //CROW_LOG_INFO << "w key fwd: " << crow::json::dump(w) << " skip: "
                //<< skip << ", limit: " << limit;
                
                allResults.push_back(std::move(w));
                
                
            }
        }
        
        for(auto& x : Sorter::backwards< std::vector<crow::json::wvalue> >(allResults)){
            i++;
            if(i > lowerbound && (i < upperbound || limit == -1)){
                try{
                    //crow::json::wvalue wb = x;
                    //CROW_LOG_INFO << "sorted object: " << i << ". " << crow::json::dump(w);
                    matchedResults.push_back(std::move(x));
                    
                }catch (const std::runtime_error& error){
                    CROW_LOG_INFO << "Runtime Sort Error 4.1: " << error.what() << " , " << i;
                }
            }
            
            if((i == upperbound) && (limit != -1)){
                break;
            }
        }
        
        
        // do something after loop
        
        delete it;
        
    }
    
    return ret && dbStatus.ok();
}

bool Core::getCount(std::string wild,
                   long& out,
                   int skip /*= 0*/, int limit /*= -1*/) {
    
    bool ret = true;
    if (dbStatus.ok()){
        out = 0;
        
        std::size_t found = wild.find("*");
        if(found != std::string::npos && found == 0){
            return false;
        }
        
        // create new iterator
        rocksdb::ReadOptions ro;
        rocksdb::Iterator* it = db->NewIterator(ro);
        
        std::string pre = wild.substr(0, found);
        
        rocksdb::Slice prefix(pre);
        
        rocksdb::Slice prefixPrint = prefix;
        CROW_LOG_INFO << "prefix : " << prefixPrint.ToString();
        
        
        int i  = -1;
        int count = (limit == -1) ? INT_MAX : limit;
        
        int lowerbound  = skip - 1;
        int upperbound = skip + count;
        
        for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
            
            if(wildcmp(wild.c_str(), it->key().ToString().c_str())){
                
                i++;
                
                if(i > lowerbound && (i < upperbound || limit == -1)){
                    
                    out++;
                }
                
                if((i == upperbound) && (limit != -1)){
                    break;
                }
                
            }
        }
        
        
        
        // do something after loop
        
        delete it;
        
    }
    
    return ret && dbStatus.ok();
}

bool Core::iter(std::string wild,
                std::vector<std::string>& matchedResults,
                    int skip /*= 0*/, int limit /*= -1*/) {
    
    bool ret = false;
    
    if (dbStatus.ok()){
        // create new iterator
        rocksdb::ReadOptions ro;
        rocksdb::Iterator* it = db->NewIterator(ro);
       
        int i  = -1;
        int count = (limit == -1) ? INT_MAX : limit;
        
        int lowerbound  = skip - 1;
        int upperbound = skip + count;
        
        // iterate all entries
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            
            std::string value = "(null)";
            rocksdb::Slice slice = it->value();
            if(slice != nullptr){
                try{
                    value = slice.ToString();
                    CROW_LOG_INFO << "iterate >> " << it->key().ToString()
                    << ": " << value; //<< endl;
                }catch(std::exception e){
                    CROW_LOG_INFO << "exception on iterate >> " << it->key().ToString();
                }
            }
            if(!(wild.size() > 0) || wildcmp(wild.c_str(), it->key().ToString().c_str())){
                
                i++;
                if(i > lowerbound && (i < upperbound || limit == -1)){
                    //CROW_LOG_INFO << "wild: " << wild << "," << result fwd: " << it->value().ToString() << " skip: "
                    //<< skip << ", limit: " << limit;
                     matchedResults.push_back(value);
                }
            
            
                if((i == upperbound) && (limit != -1)){
                    break;
                }
            }
        }
        ret = (it->status().ok());  // check for any errors found during the scan
        
        // do something after loop
        delete it;
        
       
    }
    
    return ret;
    
}


bool Core::remove(std::string key, std::string& out){
    bool ret = false;
    
    // Delete value
    if (dbStatus.ok()){
        rocksdb::ReadOptions ro;
        rocksdb::Iterator* it = db->NewIterator(ro);
        
        try{
            rocksdb::Slice prefix(key);
            //rocksdb::Slice prefixPrint = prefix;
            //CROW_LOG_INFO << "prefix : " << prefixPrint.ToString();
            
            it->Seek(prefix);
            if(it->Valid() && !key.compare(it->key().ToString())){
            
                rocksdb::Status status = db->Delete(rocksdb::WriteOptions(), key);
                
                if(status.ok()){
                    ret = true;
                    out = R"({"result":1})";
                    
                }else{
                    out = R"({"result":-1})";
                }
                
            }else{
                ret = true;
                out = R"({"result":0})";
            }
            
        }catch (const std::runtime_error& error){
            CROW_LOG_INFO << "Runtime Error: " <<  it->value().ToString();
            
            out = R"({"result":-1})";
            
        }
        
        delete it;
        
    }else{
        out = R"({"result":-1})";
    }
    
    return ret;
    
}

int Core::removeAll(std::string wild,  int skip /*= 0*/, int limit /*= -1*/){
    int out = 0;
    if (dbStatus.ok()){
        std::size_t found = wild.find("*");
        if(found != std::string::npos && found == 0){
            return out;
        }
        
        // create new iterator
        rocksdb::ReadOptions ro;
        
        std::string pre = wild.substr(0, found);
        
        rocksdb::Slice prefix(pre);
        
        rocksdb::Slice prefixPrint = prefix;
        CROW_LOG_INFO << "prefix : " << prefixPrint.ToString();
        
        int i  = -1;
        int count = (limit == -1) ? INT_MAX : limit;
        
        int lowerbound  = skip - 1;
        int upperbound = skip + count;
        
        rocksdb::Iterator* it = db->NewIterator(ro);
        
        try{
            
            rocksdb::WriteBatch batch;
            for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
                
                if(wildcmp(wild.c_str(), it->key().ToString().c_str())){
                    i++;
                    
                    if(i > lowerbound && (i < upperbound || limit == -1)){
                        CROW_LOG_INFO << "w batch del: " << it->key().ToString();
                        batch.Delete(it->key());
                        out++;
                        
                    }
                    
                    if((i == upperbound) && (limit != -1)){
                        break;
                    }
                    
                }
                
            }
            
            // do something after loop
            delete it;
            
            rocksdb::Status s = db->Write(rocksdb::WriteOptions(), &batch);
            
        }catch (const std::runtime_error& error){
            CROW_LOG_INFO << "Runtime Error: " << out << it->value().ToString();
            
             delete it;
            
        }
        
        return out;
        
    }
    
    return false;
}

bool Core::removeAtom(crow::json::rvalue& x, std::string& out) {
    
    bool ret = true;
    
    int i = 0;
    for(auto& v : x){
        ret &= remove(v.s(), out);
        if(!ret){
            break;
        }
        
        i++;
    }
    if(ret){
        out = std::string("{") + R"("result":)" + std::to_string(i) + std::string("}");
    
    }else{
        std::string error = ",\"error\":\"could not remove all keys\"";
        out = std::string("{") + R"("result":)" + std::to_string(i) + error + std::string("}");
    }
    
    return ret;
    
}

bool Core::removeAtom(std::string body, std::string& out) {
    auto x = crow::json::load(body);
    
    if (!x){
        CROW_LOG_INFO << "invalid put body" << body;
        //out = "invalid put body";
        
        out = "{\"error\": \"Invalid put body\"}";
        
        return false;
        
    }
    
    return removeAtom(x, out);
    
}


/*bool Core::putJson(std::string key, crow::json::rvalue& x, crow::json::wvalue& out) {
    
    bool ret = true;
    
    crow::json::wvalue w = std::move(x);
    std::string value = crow::json::dump(w);
    
    rocksdb::Slice keySlice = key;
    
    // modify the database
    if (dbStatus.ok()){
        
        rocksdb::Status status = db->Put(rocksdb::WriteOptions(), keySlice, value);
        if(!status.ok()){
            key = "";
            ret = false;
        }
        
    }else{
        key = "";
        ret = false;
    }
    
    std::stringstream ss;
    std::string result = ret?"true" : "false";
    ss<< "{" << std::quoted("result") << ":" << std::quoted(result) << "}";
    
    out = crow::json::load(ss.str());
    
    return  ret;
    
}*/

bool Core::getJson(std::string key, crow::json::wvalue& out){
    
    bool ret = false;
    
    /*std::map<std::string, crow::json::rvalue>::iterator it = _Core.find(key);
     if (it != _Core.end()){
     out = it->second;
     ret = true;
     }*/
    if (dbStatus.ok()){
        std::string value;
        rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &value);
        
        if(status.ok()){
            out =  crow::json::load(value);
            ret = true;
        }
    }
    
    return ret;
    
}

// to be discarded later
bool prefixIter(rocksdb::Iterator*& it, std::string wild,
                std::vector<crow::json::wvalue>& matchedResults,
                int skip /*= 0*/, int limit /*= -1*/){
    
    std::size_t found = wild.find("*");
    if(found != std::string::npos && found == 0){
        return false;
    }
    
    std::string pre = wild.substr(0, found);
    
    rocksdb::Slice prefix(pre);
    
    rocksdb::Slice prefixPrint = prefix;
    CROW_LOG_INFO << "prefix : " << prefixPrint.ToString();
    
    
    
    // Go Backwards
    //it->Seek(prefix);
    
    //CROW_LOG_INFO << "key : " <<  it->key().ToString();
    
    /*for ( ; it->Valid() && it->key().starts_with(prefix); it->Prev()) {
     if(wildcmp(wild.c_str(), it->key().ToString().c_str())){
     crow::json::wvalue w;
     auto x = crow::json::load(it->value().ToString());
     
     if(!x){
     w =  crow::json::load(std::string("[\"") +
     it->value().ToString() + std::string("\"]"));
     
     }else{
     w = x;
     }
     
     CROW_LOG_INFO << "w backwd : " << crow::json::dump(w);
     
     matchedResults.push_back(std::move(w));
     
     }
     // do something
     }*/
    
    // Go Forward
    //it->Seek(prefix); // already processed so need to start from next
    int i  = -1;
    int count = (limit == -1) ? INT_MAX : limit;
    
    int lowerbound  = skip - 1;
    int upperbound = skip + count;
    
    for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
        
        if(wildcmp(wild.c_str(), it->key().ToString().c_str())){
            crow::json::wvalue w;
            auto x = crow::json::load(it->value().ToString());
            
            if(!x){
                w =  crow::json::load(std::string("[\"") +
                                      it->value().ToString() + std::string("\"]"));
                
            }else{
                w = x;
            }
            
            i++;
            if(i > lowerbound && (i < upperbound || limit == -1)){
                CROW_LOG_INFO << "w fwd: " << crow::json::dump(w) << " skip: "
                << skip << ", limit: " << limit;
                matchedResults.push_back(std::move(w));
                
            }
            
            if((i == upperbound) && (limit != -1)){
                break;
            }
            
        }
        
    }
    
    // do something after loop
    
    
    return true;
    
}

bool Core::iterJson(std::string wild,
                    std::vector<crow::json::wvalue>& matchedResults,
                    int skip /*= 0*/, int limit /*= -1*/) {
    
    
    if (dbStatus.ok()){
        // create new iterator
        rocksdb::ReadOptions ro;
        rocksdb::Iterator* it = db->NewIterator(ro);
        //rocksdb::Iterator* it = db->NewIterator(rocksdb::ReadOptions());
        if(prefixIter(it, wild, matchedResults, skip, limit)){
            bool ret = it->status().ok();
            delete it;
            
            return ret;
        }
        
        int i  = -1;
        int count = (limit == -1) ? INT_MAX : limit;
        
        int lowerbound  = skip - 1;
        int upperbound = skip + count;
        
        // iterate all entries
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            //CROW_LOG_INFO << "iterate : " << it->key().ToString()
            //<< ": " << it->value().ToString() ; //<< endl;
            
            if(wildcmp(wild.c_str(), it->key().ToString().c_str())){
                crow::json::wvalue w;
                auto x = crow::json::load(it->value().ToString());
                //CROW_LOG_INFO << "w : " << crow::json::dump(w);
                
                if(!x){
                    w =  crow::json::load(std::string("[\"") +
                                          it->value().ToString() + std::string("\"]"));
                    
                }else{
                    w = x;
                }
                
                i++;
                if(i > lowerbound && (i < upperbound || limit == -1)){
                    CROW_LOG_INFO << "w fwd: " << crow::json::dump(w) << " skip: "
                    << skip << ", limit: " << limit;
                    matchedResults.push_back(std::move(w));
                    
                }
                
            }
        }
        //assert(it->status().ok());  // check for any errors found during the scan
        
        // do something after loop
        delete it;
        
        return it->status().ok();
    }
    
    return false;
    
}

bool Core::getList(crow::json::rvalue& args,
                      std::vector<crow::json::wvalue>& matchedResults,
                      int skip /*= 0*/, int limit /*= -1*/) {
    
    
    bool ret = true;
    
    if (dbStatus.ok()){
        
        try{
            int i  = -1;
            int count = (limit == -1) ? INT_MAX : limit;
            
            int lowerbound  = skip - 1;
            int upperbound = skip + count;
            
            crow::json::wvalue out;
            crow::json::wvalue w;
            
            // iterate all entries
            for (auto key : args) {
                std::string s = key.s();
                if(getJson(s, out)){
                    w["value"] = std::move(out);
                }else{
                    w["error"] = i+1; // i is incremented later ..
                }
                   
                w["key"] = s;
                   
               i++;
               if(i > lowerbound && (i < upperbound || limit == -1)){
                   matchedResults.push_back(std::move(w));
               }
                
            }
            
        } catch (const std::runtime_error& error){
            ret = false;
            
        }
        
    }
    
    return ret;
    
}



bool Core::searchJson(crow::json::rvalue& args,
                      std::vector<crow::json::wvalue>& matchedResults,
                      int skip /*= 0*/, int limit /*= -1*/) {
    
    
    if (dbStatus.ok()){
        
        Core& core = *this;
        
        v8Engine ve([](std::string log){
            CROW_LOG_INFO << log.c_str();
        });
        
        std::string sArgs =  crow::json::dump(args);
        CROW_LOG_INFO << "args : " << sArgs;
        
        std::string wild = args["keys"].s();
        CROW_LOG_INFO << "keys : " << wild.c_str();
        
        crow::json::wvalue wArgs;
        wArgs["include"] = args["include"];
        CROW_LOG_INFO << "include : " << crow::json::dump(wArgs);
        
        crow::json::wvalue wFilter;
        wFilter["include"]= std::move(wArgs["include"]);
        CROW_LOG_INFO << "include : " << crow::json::dump(wFilter["include"]);
        
        std::string sFilter = crow::json::dump(wFilter["include"]);
        crow::json::rvalue filter = crow::json::load(sFilter);
        
        std::string mapField = filter["map"]["field"].s();
        std::string mapAs = filter["map"]["as"].s();
        
        std::string moduleName = filter["module"].s();
        std::string funcName = filter["filter"].s();
        std::string params = filter["params"].s();
        
        auto funcOnV8EngineLoaded = [&core, &ve, &wild, &funcName, &params, &mapField, &mapAs, &matchedResults, &skip, &limit]
        (v8Engine::v8Context& v8Ctx) -> int {
            
            // create new iterator
            rocksdb::Iterator* it = db->NewIterator(rocksdb::ReadOptions());
            
            bool prefixSearch = true;
            std::size_t found = wild.find("*");
            if(found != std::string::npos && found == 0){
                prefixSearch = false;
            }
            
            rocksdb::Slice prefix(wild.substr(0, found));
            //for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next())
            
            if(!prefixSearch){
                it->SeekToFirst();
            }else{
                it->Seek(prefix);
            }
            
            int i  = -1;
            int count = (limit == -1) ? INT_MAX : limit;
            
            int lowerbound  = skip - 1;
            int upperbound = skip + count;
            
            // iterate all entries
            for (  ; prefixSearch ? (it->Valid() && it->key().starts_with(prefix)) : it->Valid();
                 it->Next()) {
               
                
                std::string elem = it->value().ToString();
                
                //CROW_LOG_INFO << "iterate : " << it->key().ToString()
                //<< ": " <<  elem; //<< endl;
                
                if(wildcmp(wild.c_str(), it->key().ToString().c_str())){
                    //crow::json::rvalue r;
                    auto r = crow::json::load(it->value().ToString());
                    if(!r){
                        r =  crow::json::load(std::string("[\"") +
                                              it->value().ToString() + std::string("\"]"));
                        
                        
                    }
                    
                    
                    //CROW_LOG_INFO << "r : " << elem;
                    
                    std::string mapKey = r[mapField.c_str()].s();
                    crow::json::wvalue mapJson;
                    
                    crow::json::wvalue w = r;
                    if(core.getJson(mapKey, mapJson)){
                        w[mapAs] = std::move(mapJson);
                    }
                    
                    //std::string allow = ve.invoke("matcher", sfilter.c_str(), //filterParams.c_str());
                    std::string e = crow::json::dump(w);
                    std::string allow = ve.invoke(v8Ctx, funcName, e, params.c_str());
                    if(atoi(allow.c_str()) == 1){
                        i++;
                        
                        ///////////////////////////////////////
                        
                        //matchedResults.push_back(std::move(w));
                        if(skip == 0 && limit == -1){
                            matchedResults.push_back(std::move(w));
                            CROW_LOG_INFO << "w fwd: " << crow::json::dump(w);
                        }else{
                            if(i > lowerbound && i < upperbound){
                                matchedResults.push_back(std::move(w));
                                CROW_LOG_INFO << "w fwd: " << crow::json::dump(w) << " skip: "
                                << skip << ", limit: " << limit;
                            }else{
                                break; // no need to search further
                            }
                        }
                        
                        ///////////////////////////////////////
                    }
                    
                }
            }
            // do something after loop
            bool ret = it->status().ok();
            delete it;
            
            return ret;
            
            
        }; // funcOnV8EngineLoaded
        
        std::string moduleJS = moduleName + ".js";
        return ve.load(moduleJS, funcOnV8EngineLoaded);
        
    }
    
    return false;
    
}

bool Core::atom(std::string body, std::string& out) {
    auto x = crow::json::load(body);
    
    if (!x){
        CROW_LOG_INFO << "invalid put body" << body;
        //out = "invalid put body";
        
        out = "{\"error\": \"Invalid put body\"}";
        
        return false;
        
    }
    
    out = std::string("{") + R"("result":false)" + std::string("}");
    
    bool ret = true;
    
    if(x.has("remove")){
        ret &= removeAtom(crow::json::dump(x["remove"]), out);
    }
    if(ret){
        if(x.has("put")){
            ret &= putAtom(crow::json::dump(x["put"]), out);
        }
    }
    
    return ret;
    
}


bool Core::fileTransfer(std::string moduleName, std::string funcName, std::string channelName,
                        std::string remoteDescription) {
    
    //Core& core = *this;
    
    v8Engine ve([](std::string log){
        CROW_LOG_INFO << log.c_str();
    });
    
    
    auto v8Loaded = [&ve, &funcName, &channelName, &remoteDescription]
    (v8Engine::v8Context& v8Ctx) -> int {
        
        std::string rawData = ve.invoke(v8Ctx, funcName, channelName, remoteDescription);
        
        //writeFile(fileName, rawData);
        CROW_LOG_INFO << rawData;
        
        return (rawData.size() > 0);
        
    };
    
    std::string moduleJS = moduleName + ".js";
    CROW_LOG_INFO << moduleJS;
    
    return ve.load(moduleJS, v8Loaded);
    // return true;
}


bool Core::openTCPSocketClient(){
    bool ret = false;
    
    boost::asio::io_service io_service;
    
    //socket creation
    tcp::socket socket(io_service);
    
    //connection
    socket.connect( tcp::endpoint( boost::asio::ip::address::from_string("127.0.0.1"), 1234 ));
    
    // request/message from client
    const std::string msg = "Hello from Client!\n";
    boost::system::error_code error;
    boost::asio::write( socket, boost::asio::buffer(msg), error );
    if( !error ) {
        CROW_LOG_INFO << "Client sent hello message!";
        ret = true;
    }
    else {
        ret = false;
        CROW_LOG_INFO << "send failed: " << error.message();
    }
    
    // getting a response from the server
    boost::asio::streambuf receive_buffer;
    boost::asio::read(socket, receive_buffer, boost::asio::transfer_all(), error);
    if( error && error != boost::asio::error::eof ) {
        ret = false;
        CROW_LOG_INFO << "receive failed: " << error.message();
    }
    else {
        ret = true;
        const char* data = boost::asio::buffer_cast<const char*>(receive_buffer.data());
        CROW_LOG_INFO << data;
    }
    
    return ret;
}

void Core::onSocketMessage(crow::websocket::connection& conn,
                           std::unordered_set<crow::websocket::connection*>& users,
                           const std::string& data, bool is_binary){
    for(auto u:users)
        if (is_binary)
            u->send_binary(data);
        else
            u->send_text(data);

}
