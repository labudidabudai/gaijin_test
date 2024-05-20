import sys
import subprocess
import os
import argparse
import signal
import time
import json


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", help="server port", type=int, required=True)
    parser.add_argument("--num_requests", type=int, help="number of requests to send", required=True)
    parser.add_argument("--request_period", type=int, help="period between requests (microseconds)", required=True)
    parser.add_argument("--num_clients", type=int, help="number of clients to simulate", required=True)
    parser.add_argument("--key_file", help="path to key file", required=True)
    args = parser.parse_args()

    # generate config.txt
    initial_keys = {}
    with open(args.key_file, "r") as f:
        lines = f.readlines()
        for line in lines:
            initial_keys[line.strip()] = line.strip()
    with open("config.txt", "w") as f:
        f.write(json.dumps(initial_keys))

    if not os.path.exists("test_res"):
        os.makedirs("test_res")
    os.system("rm -rf test_res/*")

    server_process = subprocess.Popen([
        "./dictionary_server_main",
        str(args.port),
    ])

    client_processes = []
    for i in range(int(args.num_clients)):
        print(f"Starting client {i}")
        c = subprocess.Popen([
            "./dictionary_load_client",
            "127.0.0.1",
            str(args.port),
            str(args.num_requests),
            str(args.request_period),
            args.key_file,
            f"test_res/client_{i}.txt"
        ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        client_processes.append(c)

    for c in client_processes:
        c.wait()

    server_process.send_signal(signal.SIGINT)
    server_process.wait()

    total_reads = 0
    total_writes = 0
    total_read_time = 0
    total_write_time = 0
    for f in os.listdir("test_res"):
        with open(f"test_res/{f}", "r") as f:
            data = json.loads(f.read())
            if "read" in data:
                total_reads += data["read"]["n_samples"]
                total_read_time += data["read"]["mean"] * data["read"]["n_samples"]
            if "write" in data:
                total_writes += data["write"]["n_samples"]
                total_write_time += data["write"]["mean"] * data["write"]["n_samples"]

    print(f"Read mean: {total_read_time / total_reads} us ({total_reads} samples)")
    print(f"Write mean: {total_write_time / total_writes} us ({total_writes} samples)")

    
if __name__ == "__main__":
    main()
