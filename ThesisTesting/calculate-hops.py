import sys
import csv
import numpy as np
import scipy.stats as sp
import math
import os
from collections import defaultdict


def mean_confidence_interval(data, confidence):
	a = 1.0*np.array([float(data0) for data0 in data])
	n = len(a)
	mean, std = np.mean(a), np.std(a,ddof=1)
	h = sp.norm.interval(confidence,mean,std/math.sqrt(n))
	return mean, h[1]-mean


path_topology_data = sys.argv[1]
numberOfIterations = int(sys.argv[2])

#routingProtocols = ["BMRF_BROADCAST" , "BMRF_UNICAST" ]
routingProtocols = ["UNICAST"]
topologies = ['sim-3']


dirpath = os.getcwd()
foldername = os.path.basename(dirpath)


#for each property we create a defaultdict to save the data

for topology in topologies:
    csv_header = ["hops"]
    #for each topology we have to open the specfic folder and correct files to gather the data and save them in de defaultdict
    path = foldername + "/" + topology
    allData = defaultdict(list) #this defaultdict wil save all the calculated data
            
    allDelay = defaultdict(list)
    allPDR = defaultdict(list)
        
    for routing_pr in routingProtocols:
        #csv_header.append("Delay_" + routing_pr)
        #csv_header.append("CI")
        #csv_header.append("PDR_" + routing_pr)
        #csv_header.append("CI")
        #for each routing protocol we create defaultdicts
        delay_rpr = defaultdict(list)
        pdr_rpr = defaultdict(list)

        for i in range(1,numberOfIterations +1):
            #calculate the mean and ci of all the iterations per node
            delays = defaultdict(list)
            pdr = defaultdict(list)

            print("Calculating PDRs for : " +routing_pr + "-" + topology + "-iteration-" + str(i))

            with open(topology + "/" + "pdr_results_" + routing_pr + "_" + str(i) + ".csv" ,'rU') as fdel:
                reader = csv.reader(fdel , delimiter =";" , dialect= csv.excel_tab)
                #save the data at correct node
                for row in reader:
                    pdr[int(row[2])].append(int(row[1]))
                            
                    #we calculate the confidence interval for each key pair
                    for p in pdr:
                        mean, ci = mean_confidence_interval(pdr[p],0.95)
                        print("mean = " + str(mean) + "   confidence interval = " + str(ci))
                        pdr_rpr[p].append(mean)

            print("Calculating delays for : " +routing_pr + "-" + topology + "-iteration-" + str(i))
            with open(topology + "/" + "delay_results_" + routing_pr + "_" + str(i) + ".csv" ,'rU') as fdel:
                reader = csv.reader(fdel , delimiter =";" , dialect= csv.excel_tab)
                #save the data at correct node
                for row in reader:
                    delays[int(row[3])].append(int(row[2]))
                            
                    #we calculate the confidence interval for each key pair
                    for d in delays:
                        mean, ci = mean_confidence_interval(delays[d],0.95)
                        print("mean = " + str(mean) + "   confidence interval = " + str(ci))
                        delay_rpr[d].append(mean)
                            
            print(delay_rpr)
            print(pdr_rpr)

        allPDR[routing_pr] = pdr_rpr    
        allDelay[routing_pr] = delay_rpr
        
    allData['Delay'] = allDelay
    allData['PDR'] = allPDR
        
        
    #all this data needs to be saved to csv in differect files (for each mac protocol a different file)
    #see how many nodes are used in each topology
        
    nodes_numbers = set([])
    for properties , routing_protocols in allData.items():
        for routing_protocol , nodes in routing_protocols.items():
            for node , data in nodes.items():
                nodes_numbers.add(node)
    print(nodes_numbers)
            
    nodes_numbers = list(nodes_numbers)
    nodes_numbers = np.array(nodes_numbers)
            
    totaldata = nodes_numbers.T
            
            
    for data_name , methods in allData.items():
        #print("Data_name = " + data_name)
        data_nodes = []
        data_nodes_np = np.array(data_nodes)
        for method, nodes in methods.items():
            print("Method = "+ data_name + "_" + method)
            title = data_name + "_" + method
            csv_header.append(title)
            csv_header.append("CI")
            all_nodes = []
            for node , data in nodes.items():
                mean, ci = mean_confidence_interval(data, 0.95)
                row = [mean , ci]
                all_nodes.append(row)
                        
            all_nodes_np = np.array(all_nodes)
            totaldata = np.column_stack((totaldata, all_nodes_np))
            
    filename = "results_" + topology + "-"+ ".csv"
    np.savetxt(filename, totaldata, delimiter="," , header=str(csv_header), comments='')
            
          
            
        





