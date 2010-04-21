/*
 * The script builds a cactus tree representation of the chains and nets.
 * The format of the output graph is dot format.
 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

#include "cactus.h"
#include "avl.h"
#include "commonC.h"
#include "hashTableC.h"

/*
 * Global variables.
 */
static bool edgeColours = 1;
static bool nameLabels = 0;

static void usage() {
	fprintf(stderr, "cactus_graphViewer, version 0.2\n");
	fprintf(stderr, "-a --logLevel : Set the log level\n");
	fprintf(stderr, "-c --netDisk : The location of the net disk directory\n");
	fprintf(stderr, "-d --netName : The name of the net (the key in the database)\n");
	fprintf(stderr, "-e --outputFile : The file to write the dot graph file in.\n");
	fprintf(stderr, "-f --chainColours : Do not give chains distinct colours (instead of just black)\n");
	fprintf(stderr, "-g --nameLabels : Give chain and net nodes name labels.\n");
	fprintf(stderr, "-h --help : Print this help screen\n");
}

void addEndNodeToGraph(End *end, FILE *fileHandle) {
	const char *nameString = netMisc_nameToStringStatic(end_getName(end));
	graphViz_addNodeToGraph(nameString, fileHandle, nameString, 0.5, 0.5, "circle", "black", 14);
}

void addEdgeToGraph(End *end1, End *end2, const char *colour, const char *label, double length, double weight, const char *direction, FILE *fileHandle) {
	char *nameString1 = netMisc_nameToString(end_getName(end1));
	char *nameString2 = netMisc_nameToString(end_getName(end2));
	graphViz_addEdgeToGraph(nameString1, nameString2, fileHandle, nameLabels ? label : "", colour, length, weight, direction);
	free(nameString1);
	free(nameString2);
}

void addBlockToGraph(Block *block, const char *colour, FILE *fileHandle) {
	static char label[100000];
	End *leftEnd = block_get5End(block);
	End *rightEnd = block_get3End(block);
	addEndNodeToGraph(leftEnd, fileHandle);
	addEndNodeToGraph(rightEnd, fileHandle);
	Block_InstanceIterator *iterator = block_getInstanceIterator(block);
	Segment *segment;
	while((segment = block_getNext(iterator)) != NULL) {
		segment = segment_getStrand(segment) ? segment : segment_getReverse(segment);
		if(segment_getSequence(segment) != NULL) {
			sprintf(label, "%s:%i:%i", netMisc_nameToStringStatic(sequence_getName(segment_getSequence(segment))),
					segment_getStart(segment), segment_getStart(segment)+segment_getLength(segment));
			addEdgeToGraph(cap_getEnd(segment_get5Cap(segment)),
						   cap_getEnd(segment_get3Cap(segment)),
						   edgeColours ? colour : "black", label, 1.5, 100, "forward", fileHandle);
		}
	}
	block_destructInstanceIterator(iterator);
}

void addTrivialChainsToGraph(Net *net, FILE *fileHandle) {
	/*
	 * Add blocks not part of chain to the graph
	 */
	Net_BlockIterator *blockIterator = net_getBlockIterator(net);
	Block *block;
	while((block = net_getNextBlock(blockIterator)) != NULL) {
		if(block_getChain(block) == NULL) {
			addBlockToGraph(block, "black", fileHandle);
		}
	}
	net_destructBlockIterator(blockIterator);
}

void addChainsToGraph(Net *net, FILE *fileHandle) {
	/*
	 * Add blocks part of a chain to the graph.
	 */
	Net_ChainIterator *chainIterator = net_getChainIterator(net);
	Chain *chain;
	while((chain = net_getNextChain(chainIterator)) != NULL) {
		int32_t i, j;
		const char *chainColour;
		while((chainColour = graphViz_getColour(chainColour)) != NULL) { //ensure the chain colours don't match the trivial block chains and the adjacencies.
			if(strcmp(chainColour, "black") != 0 && strcmp(chainColour, "grey") != 0) {
				break;
			}
		}
		Block **blocks = chain_getBlockChain(chain, &i);
		for(j=0; j<i; j++) {
			addBlockToGraph(blocks[j], chainColour, fileHandle);
		}
		free(blocks);
	}
	net_destructChainIterator(chainIterator);
}

void addAdjacencies(Group *group, FILE *fileHandle) {
	/*
	 * Adds adjacency edges to the graph.
	 */
	static char label[10000];
	Group_EndIterator *endIterator = group_getEndIterator(group);
	End *end;
	Net *net = group_getNet(group);
	while((end = net_getNextEnd(endIterator)) != NULL) {
		End_InstanceIterator *instanceIterator = end_getInstanceIterator(end);
		Cap *cap;
		char *netName = netMisc_nameToString(net_getName(net));
		while((cap = end_getNext(instanceIterator)) != NULL) {
			if(cap_getSequence(cap) != NULL) {
				cap = cap_getStrand(cap) ? cap : cap_getReverse(cap);
				Cap *cap2 = cap_getAdjacency(cap);
				if(!cap_getSide(cap)) {
					assert(cap_getCoordinate(cap) < cap_getCoordinate(cap2));
					sprintf(label, "%s:%i:%i:%s:%i", netMisc_nameToStringStatic(sequence_getName(cap_getSequence(cap))),
							cap_getCoordinate(cap), cap_getCoordinate(cap2),
							netName,
							net_getEndNumber(net));
					//sprintf(label, "%s:%i",
					//		netName,
					//		net_getEndNumber(net));
					addEdgeToGraph(cap_getEnd(cap), cap_getEnd(cap2), "grey", label, 1.5, 1, "forward", fileHandle);
				}
			}
		}
		free(netName);
		end_destructInstanceIterator(instanceIterator);
	}
	group_destructEndIterator(endIterator);
}

void addStubAndCapEndsToGraph(Net *net, FILE *fileHandle) {
	Net_EndIterator *endIterator = net_getEndIterator(net);
	End *end;
	while((end = net_getNextEnd(endIterator)) != NULL) {
		if(end_isStubEnd(end)) {
			addEndNodeToGraph(end, fileHandle);
		}
	}
	net_destructEndIterator(endIterator);
}

void makeCactusGraph(Net *net, FILE *fileHandle) {
	if(net_getParentGroup(net) == NULL) {
		addStubAndCapEndsToGraph(net, fileHandle);
	}
	addTrivialChainsToGraph(net, fileHandle);
	addChainsToGraph(net, fileHandle);
	Net_GroupIterator *groupIterator = net_getGroupIterator(net);
	Group *group;
	while((group = net_getNextGroup(groupIterator)) != NULL) {
		Net *nestedNet = group_getNestedNet(group);
		if(nestedNet != NULL) {
			makeCactusGraph(nestedNet, fileHandle);
		}
		else { //time to add the adjacencies!
			addAdjacencies(group, fileHandle);
		}
	}
	net_destructGroupIterator(groupIterator);
}

int main(int argc, char *argv[]) {
	NetDisk *netDisk;
	Net *net;
	FILE *fileHandle;

	/*
	 * Arguments/options
	 */
	char * logLevelString = NULL;
	char * netDiskName = NULL;
	char * netName = NULL;
	char * outputFile = NULL;

	///////////////////////////////////////////////////////////////////////////
	// (0) Parse the inputs handed by genomeCactus.py / setup stuff.
	///////////////////////////////////////////////////////////////////////////

	while(1) {
		static struct option long_options[] = {
			{ "logLevel", required_argument, 0, 'a' },
			{ "netDisk", required_argument, 0, 'c' },
			{ "netName", required_argument, 0, 'd' },
			{ "outputFile", required_argument, 0, 'e' },
			{ "edgeColours", no_argument, 0, 'f' },
			{ "nameLabels", no_argument, 0, 'g' },
			{ "help", no_argument, 0, 'h' },
			{ 0, 0, 0, 0 }
		};

		int option_index = 0;

		int key = getopt_long(argc, argv, "a:c:d:e:fgh", long_options, &option_index);

		if(key == -1) {
			break;
		}

		switch(key) {
			case 'a':
				logLevelString = stringCopy(optarg);
				break;
			case 'c':
				netDiskName = stringCopy(optarg);
				break;
			case 'd':
				netName = stringCopy(optarg);
				break;
			case 'e':
				outputFile = stringCopy(optarg);
				break;
			case 'f':
				edgeColours = !edgeColours;
				break;
			case 'g':
				nameLabels = !nameLabels;
				break;
			case 'h':
				usage();
				return 0;
			default:
				usage();
				return 1;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// (0) Check the inputs.
	///////////////////////////////////////////////////////////////////////////

	assert(netDiskName != NULL);
	assert(netName != NULL);
	assert(outputFile != NULL);

	//////////////////////////////////////////////
	//Set up logging
	//////////////////////////////////////////////

	if(logLevelString != NULL && strcmp(logLevelString, "INFO") == 0) {
		setLogLevel(LOGGING_INFO);
	}
	if(logLevelString != NULL && strcmp(logLevelString, "DEBUG") == 0) {
		setLogLevel(LOGGING_DEBUG);
	}

	//////////////////////////////////////////////
	//Log (some of) the inputs
	//////////////////////////////////////////////

	logInfo("Net disk name : %s\n", netDiskName);
	logInfo("Net name : %s\n", netName);
	logInfo("Output graph file : %s\n", outputFile);

	//////////////////////////////////////////////
	//Load the database
	//////////////////////////////////////////////

	netDisk = netDisk_construct(netDiskName);
	logInfo("Set up the net disk\n");

	///////////////////////////////////////////////////////////////////////////
	// Parse the basic reconstruction problem
	///////////////////////////////////////////////////////////////////////////

	net = netDisk_getNet(netDisk, netMisc_stringToName(netName));
	logInfo("Parsed the top level net of the cactus tree to build\n");

	///////////////////////////////////////////////////////////////////////////
	// Build the graph.
	///////////////////////////////////////////////////////////////////////////

	fileHandle = fopen(outputFile, "w");
	graphViz_setupGraphFile(fileHandle);
	makeCactusGraph(net, fileHandle);
	graphViz_finishGraphFile(fileHandle);
	fclose(fileHandle);
	logInfo("Written the tree to file\n");

	///////////////////////////////////////////////////////////////////////////
	// Clean up.
	///////////////////////////////////////////////////////////////////////////

	netDisk_destruct(netDisk);

	return 0;
}