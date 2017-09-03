# simple-http-server

A simple http server that can handle GET and POST request. 

Written for computer network course. 

## GET method

GET method works like a normal HTTP server, but not setting  `index.*`  as the default index file. 

As this server is just for the course, it maybe inefficient and insecure.  

I used `sendfile()` to send the requested files, and mime type is judged by the extension name. 

## POST method

POST method is totally implemented for the lab requirements. 

It will verify the username and password post to the `/dopost`.

## How-to

```shell
make httpserver
./httpserver <public_dir>
```
## Attribution

- https://github.com/dermesser/libsocket
- https://github.com/Pithikos/C-Thread-Pool
- https://github.com/Menghongli/C-Web-Server/blob/master/get-mime-type.c