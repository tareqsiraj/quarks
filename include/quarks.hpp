#ifndef _QUARKS_H_
#define _QUARKS_H_

#include <string>
#include <map>

#include <crow.h>

/**
 * @brief This namespace refers to implementation of core functionalities from the service
 * 
 */
namespace Quarks {

    /**
     * @brief Solves the shortcomings of Redis - Sort, Expiry, Serialize
     * 
     */
    class Core {
        public:
        static Core _Instance;
        
        Core();
        ~Core();
        
        void setEnvironment(int argc, char** argv);
        void shutDown();
        
        int getPort();
        
        bool insert(bool failIfExists, std::string body, std::string& out);
        bool post(std::string body, std::string& out);
        bool put(std::string body, std::string& out);
        bool putAtom(crow::json::rvalue& x, std::string& out);
        bool putAtom(std::string body, std::string& out);

        bool exists(std::string key, std::string& out);
        bool get(std::string key, std::string& value);
        
        bool getAll(std::string wild,
                            std::vector<crow::json::wvalue>& matchedResults,
                            int skip = 0, int limit = -1);
        
        bool getSorted(std::string wild, std::string sortby, bool ascending,
                     std::vector<crow::json::wvalue>& matchedResults,
                     int skip = 0, int limit = -1);

		bool getKeys(std::string wild,
                    std::vector<crow::json::wvalue>& matchedResults,
                    int skip = 0, int limit = -1);
        
        bool getKeysReversed(std::string wild,
                     std::vector<crow::json::wvalue>& matchedResults,
                     int skip = 0, int limit = -1);
        
        bool getCount(std::string wild,
                     long& out,
                     int skip = 0, int limit = -1);
        
        bool iter(std::string wild,
                  std::vector<std::string>& matchedResults,
                      int skip = 0, int limit = -1);
        
        bool remove(std::string key, std::string& out);
        int removeAll(std::string wild, int skip = 0, int limit = -1);
        bool removeAtom(crow::json::rvalue& x, std::string& out);
        bool removeAtom(std::string body, std::string& out);
        
        //bool putJson(std::string key, crow::json::rvalue& x, crow::json::wvalue& out);
        
        bool getJson(std::string key, crow::json::wvalue& out);
        
        
        bool iterJson(std::string wild,
                      std::vector<crow::json::wvalue>& matchedResults,
                      int skip = 0, int limit = -1);
    
        bool getList(crow::json::rvalue& args,
                     std::vector<crow::json::wvalue>& matchedResults,
                     int skip = 0, int limit = -1);
        
        bool searchJson(crow::json::rvalue& args,
                        std::vector<crow::json::wvalue>& matchedResults,
                        int skip = 0, int limit = -1);
        
        bool atom(std::string body, std::string& out);
        
        /////// r&d ////////////////

        bool openTCPSocketClient();
        
        bool fileTransfer(std::string moduleName, std::string funcName,
			std::string channelName, std::string remoteDescription);
        
               
        private:
        
        
        int _argc;
        std::vector<std::string> _argv;
        
        //std::map<std::string, crow::json::rvalue> _Core;
        int _portNumber;
        
    };
    
    
    
}

#include <matrix.hpp>

#endif
