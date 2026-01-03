import argparse
import http.server
import os
import ssl

def start_https_server(
    ota_image_dir: str,
    server_ip: str,
    server_port: int,
    server_file: str = None,
    key_file: str = None,
) -> None:
    """Start an HTTPS server to serve OTA images.
        Args:
            ota_image_dir (str): Directory containing OTA images.
            server_ip (str): IP address to bind the server.
            server_port (int): Port to bind the server.
            server_file (str, optional): Path to the server certificate file. Defaults to None.
            key_file (str, optional): Path to the server key file. Defaults to None.
"""

    current_dir = os.path.abspath(".")
    server_address = (server_ip, server_port)

    os.chdir(ota_image_dir)
    httpd = http.server.HTTPServer(server_address, http.server.SimpleHTTPRequestHandler)

    if server_file is not None and key_file is not None:
        server_file_path = current_dir + "/" + server_file
        key_file_path = current_dir + "/" + key_file
        ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ssl_context.load_cert_chain(certfile=server_file_path, keyfile=key_file_path)

        httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)

    httpd.serve_forever()

if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(description="Runs an HTTPS server to serve Images for Over the Air (OTA) updates.")
    arg_parser.add_argument(
        "--ota_image_dir",
        type=str,
        required=False,
        default="../../build_app/",
        help="Directory containing OTA images. Pointing to the build folder.",
    )
    arg_parser.add_argument(
        "--server_ip",
        type=str,
        required=False,
        default="0.0.0.0",
        help="IP address to bind the server.",
    )
    arg_parser.add_argument(
        "--server_port",
        type=int,
        required=False,
        default=8070,
        help="Port to bind the server.",
    )
    arg_parser.add_argument(
        "--server_cert",
        type=str,
        nargs="?",
        required=False,
        default="./ca_cert.pem",
        help="Path to the server certificate file.",)

    arg_parser.add_argument(
        "--server_key",
        type=str,
        nargs="?",
        required=False,
        default="./ca_key.pem",
        help="Path to the server key file.",
    )
    args = arg_parser.parse_args()
    print(f"Start Server with {args}")
    cwd = os.path.realpath(os.getcwd())
    file_path = os.path.realpath(__file__)
    if cwd is not file_path:
        print(f"Warning: The script was not started from the correct directory. "
            f"This might lead to not working properly. "
            f"Start from {file_path} to not get this warning."
        )

    start_https_server(
        ota_image_dir=args.ota_image_dir,
        server_ip=args.server_ip,
        server_port=args.server_port,
        server_file=args.server_cert,
        key_file=args.server_key,
    )
