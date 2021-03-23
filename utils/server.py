import socket
import sys

def main():
    sock = socket.socket()

    sock.bind(("localhost", 8080))

    while True:
        sock.listen(1)
        connection, remote_addr = sock.accept()

        print(f"Receieved connection {connection} from {remote_addr}")

        data = connection.recv(1024)

        print(f"Received {data}")

        s = data.decode('utf-8')
        capped = s.upper()
        connection.send(capped.encode('utf-8'))


if __name__ == "__main__":
    main()
