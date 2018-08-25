Modern object oriented software consists of many individual components which are required
to communicate with each other. Especially in reactive systems communication is an essential
part as every action triggers certain well-defined reactions from the entire system. External
and internal events have to be distributed among different system components. However, the
communication between those components should not be a direct communication because this
couples the components together while they should be independent. If a system’s components
depend directly upon each other they cannot easily be designed, created, tested or replaced
individually. Especially in larger scale systems this is problematic. So the goal is to achieve
a high degree of loose coupling but still manage an efficient communication between the
individual components.

Bran Selic invented the ROOM methodology in 1996 [3] which predates UML and introduces
ports as bidirectional communication interfaces. The ROOM model is relatively complex and
mostly applicable for large-scale systems but the basic idea of channelling all communications
through ports can be applied to basic communication models as well.
In order to create a connection between two independent objects for communication three
components must be designed (Figure 3.1):
• An out-port as the source of the data to be transferred
• An in-port as the destination
• A connector to forward the data from out-port to in-port
