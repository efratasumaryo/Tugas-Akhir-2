from graphviz import Digraph

# Create a directed graph for the flowchart
flowchart = Digraph(format='png', comment='Code Flowchart with Style', engine='dot')

# Main subgraph for setup and tasks
with flowchart.subgraph(name='cluster_main') as main_cluster:
    main_cluster.attr(style='filled', color='lightgrey')
    main_cluster.node_attr.update(style='filled', color='white')
    
    main_cluster.node('A', 'Start\n(Setup)')
    main_cluster.node('B', 'WiFi Setup')
    main_cluster.node('C', 'Thermocouple Check')
    main_cluster.node('D', 'Create Tasks\n(SensorTask, MQTTTask)')
    main_cluster.node('E', 'Loop Forever')
    main_cluster.edge('A', 'B')
    main_cluster.edge('B', 'C')
    main_cluster.edge('C', 'D')
    main_cluster.edge('D', 'E')
    main_cluster.attr(label='Main Flow')

# Subgraph for SensorTask
with flowchart.subgraph(name='cluster_task1') as task1_cluster:
    task1_cluster.attr(style='filled', color='lightblue')
    task1_cluster.node_attr.update(style='filled', color='white')
    
    task1_cluster.node('T1_A', 'SensorTask Start')
    task1_cluster.node('T1_B', 'Sample Sensors at 100Hz')
    task1_cluster.node('T1_C', 'Calculate Averages\n(Every 10 Samples)')
    task1_cluster.node('T1_D', 'Signal New Data')
    task1_cluster.edge('T1_A', 'T1_B')
    task1_cluster.edge('T1_B', 'T1_C', label='10ms Interval')
    task1_cluster.edge('T1_C', 'T1_D', label='If 10 Samples')
    task1_cluster.edge('T1_D', 'T1_A')
    task1_cluster.attr(label='SensorTask')

# Subgraph for MQTTTask
with flowchart.subgraph(name='cluster_task2') as task2_cluster:
    task2_cluster.attr(style='filled', color='lightgreen')
    task2_cluster.node_attr.update(style='filled', color='white')
    
    task2_cluster.node('T2_A', 'MQTTTask Start')
    task2_cluster.node('T2_B', 'MQTT Connection')
    task2_cluster.node('T2_C', 'Publish Sensor Data')
    task2_cluster.edge('T2_A', 'T2_B')
    task2_cluster.edge('T2_B', 'T2_C', label='If Connected')
    task2_cluster.edge('T2_C', 'T2_A')
    task2_cluster.attr(label='MQTTTask')

# Global edges connecting clusters
flowchart.node('Start', shape='Mdiamond', label='Start')
flowchart.node('End', shape='Msquare', label='End')
flowchart.edge('Start', 'A')
flowchart.edge('D', 'T1_A', label='SensorTask')
flowchart.edge('D', 'T2_A', label='MQTTTask')
flowchart.edge('T1_D', 'T2_C', label='Data Processed')
flowchart.edge('T2_C', 'End')

# Render and display the flowchart
flowchart.render('/mnt/data/styled_code_flowchart', view=True)
