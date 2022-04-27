import re
import subprocess
import copy
import os
import json
import time
import argparse
 
parser = argparse.ArgumentParser(description="Allows for setting up the scalable benchmark",
                                 formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument("-Pm", "--proc-manager", type=int, required=True, help="number of manager processes per node")
parser.add_argument("-Pc", "--proc-client", type=int, required=True, help="number of client processes per node")
parser.add_argument("-Nc", "--nodes-client", type=int, required=True, help="number of nodes assigned to the client")
parser.add_argument("-c", "--config", default="multiple_configs", help="location of the config files")
parser.add_argument("-r", "--results", default="multiple_results", help="where to save the results")
args = parser.parse_args()
config = vars(args)

# Number of executor manager processes per node
P_manager = config["proc_manager"]

# Number of client processes per node
P_client = config["proc_client"]

# Number of client nodes (the rest are executor nodes)
N_client = config["nodes_client"]

# Obtain the ips and hostnames
print("Running srun -l ip a")
process = subprocess.run('srun -l ip a', shell=True, stdout=subprocess.PIPE, universal_newlines=True)
ip = process.stdout
print("Running srun -l hostname")
process = subprocess.run('srun -l hostname', shell=True, stdout=subprocess.PIPE, universal_newlines=True)
hostname = process.stdout

# Parse the hostnames and ips
pattern = "(\d+)(?::.*\d+:.*ipogif0.*[\n]*.*[\n]*.*inet.)(\d{1,3}.\d{1,3}.\d{1,3}.\d{1,3})"
ips = dict(re.findall(pattern, ip))
pattern = "(\d+)(?::.*)(nid\d+)"
hostnames = dict(re.findall(pattern, hostname))

# Parse executor and client nodes
devices = {}
for key in ips.keys():
    devices[key] =  {"ip": ips[key], "hostname": hostnames[key]}

# Check total number of nodes
N_total = len(ips.keys())
N_manager = N_total - N_client
P_total = N_client*P_client + N_manager*P_manager
if N_client*P_client > int(P_total/2):
    N_client = int(P_total/2/P_client)
    N_manager = N_total - N_client

# Print status
print(f"The following nodes were registered {devices} \n")
print(f"We are spawning {N_total} nodes, {N_client} for clients and {N_manager} for managers.")
print(f"We are spawning {P_total} processses, {P_client} per machine for clients and {P_manager} per machine for managers.")

# Define templates
template_devices = {
  "devices": [
    {
      "name": "",
      "ip_address": "",
      "port": 0,
      "max_inline_data": 0,
      "default_receive_buffer_size": 20
    }
  ]
}

template_manager = {
  "config": {
    "rdma_device": "",
    "rdma_device_port": 0,
    "resource_manager_address": "127.0.0.1",
    "resource_manager_port": 0,
    "resource_manager_secret": 0
  },
  "executor": {
    "use_docker": False,
    "repetitions": 10000,
    "warmup_iters": 0,
    "pin_threads": False
  }
}

executors = {
    "executors": []
}

# Create device lists and configurations for managers
manager_node_list = []
for i in range(N_manager):
    for j in range(P_manager):
        device = copy.deepcopy(template_devices)
        manager = copy.deepcopy(template_manager)
        label = list(devices.keys())[i]
        manager_node_list.append(devices[label]["hostname"])
        device["devices"][0]["ip_address"] = devices[label]["ip"]
        device["devices"][0]["port"] = 50000 + j
        manager["config"]["rdma_device_port"] = 50000 + j
        json_string = json.dumps(device)
        with open(f'{os.path.join(config["config"], f"devices_manager_{i*P_manager+j}.json")}', 'w') as outfile:
            outfile.write(json_string)
        json_string = json.dumps(manager)
        with open(f'{os.path.join(config["config"], f"executor_manager_{i*P_manager+j}.json")}', 'w') as outfile:
            outfile.write(json_string)
            
print("Created device lists and configurations for managers")

# Create the executors list
for i in range(N_manager):
    for j in range(P_manager):
        device = {}
        label = list(devices.keys())[i]
        device["address"] = devices[label]["ip"]
        device["port"] = 50000 + j
        device["cores"] = 1
        executors["executors"].append(device)
json_string = json.dumps(executors)
with open(f'{os.path.join(config["config"], "executors_database.json")}', 'w') as outfile:
    outfile.write(json_string)
    
print("Created executors database")

# Create device lists for the clients
client_node_list = []
for i in range(0, N_client):
    for j in range(P_client):
        device = copy.deepcopy(template_devices)
        label = list(devices.keys())[i+N_manager]
        client_node_list.append(devices[label]["hostname"])
        device["devices"][0]["ip_address"] = devices[label]["ip"]
        device["devices"][0]["port"] = 50000 + j
        json_string = json.dumps(device)
        with open(f'{os.path.join(config["config"], f"/devices_client_{i*P_client+j}.json")}', 'w') as outfile:
            outfile.write(json_string)
        
print("Created device lists for clients")

# Run the managers
node_list = ",".join(manager_node_list)
command = f"""srun -l -t 00:02:00 -ntasks-per-node {P_manager} -n {N_manager*P_manager} -N {N_manager} \
              --cpu-bind=cores,verbose --oversubscribe -o managers_%t.o -e managers_%t.e --nodelist={node_list} \
              PATH=/users/mchrapek/rFaaS_old_test/rFaaS/bin:$PATH bin/executor_manager -c {os.path.join(config["config"], "executor_manager_%t.json")} \
              --device-database {os.path.join(config["config"], "devices_manager_%t.json")} --skip-resource-manager > managers_%t.o"""
print("Running the managers with command\n")
print(f"{command}\n")
manager_process = subprocess.Popen(command, shell=True) 

# Sleep for 10s to allow managers to start listening
time.sleep(10)
print("Waited 10s for managers")  

# Run the clients
node_list = ",".join(client_node_list)
command = f"""srun -l -t 00:02:00 -ntasks-per-node {P_client} -n {N_client*P_client} -N {N_client} --oversubscribe -o clients_%t.o -e clients_%t.e \
              --nodelist={node_list} benchmarks/scalable_benchmarker --config {os.path.join(config["config"], "benchmark.json")} \
              --device-database {os.path.join(config["config"], "devices_client_%t.json")} --name empty --functions examples/libfunctions.so \
              --executors-database {os.path.join(config["config"], "executors_database.json")} -s 100 \
              --output-stats {os.path.join(config["results"], "devices_client_%t.csv")} > clients_%t.o"""
print("Running the clients with command\n")
print(f"{command}\n")
client_process = subprocess.Popen(command, shell=True)
client_process.wait()
manager_process.kill()

