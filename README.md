
# quarks
Modern C++ based server side framework for optimal solutions

The philosophy and technological guidance behind Quarks can be found in this link:
[quarks philosophy](https://dev.to/lucpattyn/quarks-a-new-approach-with-a-new-mindset-to-programming-10lk)

This is an example using [Crow](https://github.com/ipkn/crow) library  to serve an OpenCV image filter. This example
users a compiled version of OpenCV 4.0.0 and requires following packages:

- OpenCV 4.0.0
- Crow Library v0.1
- GCC with support to C++17
- Cmake 1.13
- Boost::System

## How to build

After building and install OpenCV 4.0.0, GCC, Cmake and Boost::System you should.

```
mkdir build
cd build
cmake ..
make -$(nproc)
```

### Testing

After initializing by issuing following command

```
./ocv_microservice_crow
```

You should submit a POST request to http://localhost:18080/filters/gausian. The body of this request should be your
binary PNG image. The response should be a gausian filtered image from the submited image.

This however is not the purpose behind Quarks. It's just to get things started..
