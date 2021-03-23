import http.server
import json


class AwsLambdaHttpRequestHandler(http.server.BaseHTTPRequestHandler):

    def do_GET(self):
        self.protocol_version = "HTTP/1.1"
        if self.path == "/runtime/invocation/next":
            body = json.dumps({"banana": 10}).encode("utf-8")
            body_length = len(body)

            self.send_response(200)
            self.send_header("Lambda-Runtime-Aws-Request-Id", "8476a536-e9f4-11e8-9739-2dfe598c3fcd")
            self.send_header("Lambda-Runtime-Deadline-Ms", "1542409706888")
            self.send_header("Lambda-Runtime-Invoked-Function-Arn", "arn:aws:lambda:us-east-2:123456789012:function:custom-runtime")
            self.send_header("Lambda-Runtime-Trace-Id", "Root=1-5bef4de7-ad49b0e87f6ef6c87fc2e700;Parent=9a9197af755a6419;Sampled=1")
            self.send_header("Content-Length", body_length)
            self.end_headers()

            self.wfile.write(body)
            
        else:
            self.send_error(400, "Unknown path")

    def do_POST(self):
        pass


def main():
    server_address = ('', 8080)
    httpd = http.server.HTTPServer(server_address, AwsLambdaHttpRequestHandler)
    httpd.serve_forever()


if __name__ == "__main__":
    main()
