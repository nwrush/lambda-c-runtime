# AWS Lambda C runtime

## What is this?
Ever wish you could write a lambda function in C? I haven't either but I'm not going to let something as trivial as a lack valid usecases stop me. 

Now you can write your serverless code in C and easily run it in lambda.

## Why is this?
I wanted to write a lambda runtime in asm but implementing a TCP/HTTP client in assembly sounds awful.

## How do I use this?
1. Compile your code into a shared library by passing ```-fPIC``` into GCC. I'd also recommend including ```include/lambda_runtime.h``` to make reading the context object passed into your handler not incredibly painful. Place the build artifacts into ```./dist```.
2. Compile the runtime with the included ```./compile.sh```
3. Use ```./create-deployment-zip.sh``` to bundle ```./dist``` into a zip file you can upload to lambda.
4. Upload the zip to lambda
5. Set the lambda handler name to ```$FULLNAME_OF_YOUR_LIBRARY:$HANDLER_FUNCTION_NAME```. The name needs to be the relative path to your library from the runtime executable. So if your library is called foo.so which is inside a folder ```lib```, the handler value would be ```lib/foo.so:handler```.

## Should I use this?
I can't imagine any usecase where this would be useful, so if you find one please let me know. However, I didn't test this that extensively, so if you build a production system on top of this and it breaks you got what you paid for.

## Disclaimers
This project is not supported by, owned by, or recommended by AWS in any official capacity.