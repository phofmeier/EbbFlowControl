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
        ota_image_dir: Directory containing OTA images.
        server_ip: IP address to bind the server.
        server_port: Port to bind the server.
        server_file: Server certificate path (relative paths are from the
            process working directory before any chdir).
        key_file: Private key path for the server certificate.
    """
    cwd_before_chdir = os.path.abspath(os.getcwd())
    server_address = (server_ip, server_port)

    os.chdir(ota_image_dir)
    httpd = http.server.HTTPServer(
        server_address, http.server.SimpleHTTPRequestHandler
    )

    if server_file is not None and key_file is not None:
        server_file_path = os.path.join(cwd_before_chdir, server_file)
        key_file_path = os.path.join(cwd_before_chdir, key_file)
        ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ssl_context.load_cert_chain(certfile=server_file_path, keyfile=key_file_path)

        httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)

    httpd.serve_forever()


if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(
        description="Runs an HTTPS server to serve images for over-the-air (OTA) updates."
    )
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
        help="Path to the server certificate file.",
    )

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
    script_dir = os.path.dirname(os.path.realpath(__file__))
    cwd = os.path.realpath(os.getcwd())
    if cwd != script_dir:
        print(
            "Warning: Run this script from its own directory so default "
            f"certificate paths resolve correctly. Expected cwd: {script_dir}, "
            f"actual: {cwd}."
        )

    start_https_server(
        ota_image_dir=args.ota_image_dir,
        server_ip=args.server_ip,
        server_port=args.server_port,
        server_file=args.server_cert,
        key_file=args.server_key,
    )
