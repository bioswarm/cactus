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
 * The script outputs a maf file containing all the block in a net and its descendants.
 */

char *formatSequenceHeader(Sequence *sequence) {
	const char *sequenceHeader = sequence_getHeader(sequence);
	if(strlen(sequenceHeader) > 0) {
		char *cA = malloc(sizeof(char) *(1 + strlen(sequenceHeader)));
		sscanf(sequenceHeader, "%s", cA);
		return cA;
	}
	else {
		return netMisc_nameToString(sequence_getName(sequence));
	}
}

static void getMAFBlockP(Segment *segment, FILE *fileHandle) {
	assert(segment != NULL);
	Sequence *sequence = segment_getSequence(segment);
	if(sequence != NULL) {
		char *sequenceHeader = formatSequenceHeader(sequence);
		int32_t start;
		if(segment_getStrand(segment)) {
			start = segment_getStart(segment) - sequence_getStart(sequence);
		}
		else { //start with respect to the start of the reverse complement sequence
			start = (sequence_getStart(sequence) + sequence_getLength(sequence) - 1) - segment_getStart(segment);
		}
		int32_t length = segment_getLength(segment);
		char *strand = segment_getStrand(segment) ? "+" : "-";
		int32_t sequenceLength = sequence_getLength(sequence);
		char *instanceString = segment_getString(segment);
		fprintf(fileHandle, "s\t%s\t%i\t%i\t%s\t%i\t%s\n", sequenceHeader, start, length, strand, sequenceLength, instanceString);
		free(instanceString);
		free(sequenceHeader);
	}
	int32_t i;
	for(i=0; i<segment_getChildNumber(segment); i++) {
		getMAFBlockP(segment, fileHandle);
	}
}

void getMAFBlock(Block *block, FILE *fileHandle) {
	/*
	 * Outputs a MAF representation of the block to the given file handle.
	 */
	if(block_getInstanceNumber(block) > 0) {
		fprintf(fileHandle, "a score=%i\n", block_getLength(block) *block_getInstanceNumber(block));
		char *newickTreeString = block_makeNewickString(block, 1);
		fprintf(fileHandle, "s tree=%s\n", newickTreeString);
		free(newickTreeString);
		getMAFBlockP(block_getRootInstance(block), fileHandle);
	}
}

void getMAFs(Net *net, FILE *fileHandle) {
	/*
	 * Outputs MAF representations of all the block sin the net and its descendants.
	 */

	//Make MAF blocks for each block
	Net_BlockIterator *blockIterator = net_getBlockIterator(net);
	Block *block;
	while((block = net_getNextBlock(blockIterator)) != NULL) {
		getMAFBlock(block, fileHandle);
	}
	net_destructBlockIterator(blockIterator);

	//Call child nets recursively.
	Net_GroupIterator *groupIterator = net_getGroupIterator(net);
	Group *group;
	while((group = net_getNextGroup(groupIterator)) != NULL) {
		Net *nestedNet = group_getNestedNet(group);
		if(nestedNet != NULL) {
			getMAFs(group_getNestedNet(group), fileHandle); //recursive call.
		}
	}
	net_destructGroupIterator(groupIterator);
}

void makeMAFHeader(Net *net, FILE *fileHandle) {
	fprintf(fileHandle, "##maf version=1 scoring=N/A\n");
	char *cA = eventTree_makeNewickString(net_getEventTree(net));
	fprintf(fileHandle, "# cactus %s\n\n", cA);
	free(cA);
}

void usage() {
	fprintf(stderr, "cactus_mafGenerator, version 0.2\n");
	fprintf(stderr, "-a --logLevel : Set the log level\n");
	fprintf(stderr, "-c --netDisk : The location of the net disk directory\n");
	fprintf(stderr, "-d --netName : The name of the net (the key in the database)\n");
	fprintf(stderr, "-e --outputFile : The file to write the MAFs in.\n");
	fprintf(stderr, "-f --includeTrees : Include trees for each MAF block inside of a comment line.\n");
	fprintf(stderr, "-h --help : Print this help screen\n");
}

int main(int argc, char *argv[]) {
	NetDisk *netDisk;
	Net *net;

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
			{ "help", no_argument, 0, 'h' },
			{ 0, 0, 0, 0 }
		};

		int option_index = 0;

		int key = getopt_long(argc, argv, "a:c:d:e:h", long_options, &option_index);

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
	logInfo("Output MAF file : %s\n", outputFile);

	//////////////////////////////////////////////
	//Load the database
	//////////////////////////////////////////////

	netDisk = netDisk_construct(netDiskName);
	logInfo("Set up the net disk\n");

	///////////////////////////////////////////////////////////////////////////
	// Parse the basic reconstruction problem
	///////////////////////////////////////////////////////////////////////////

	net = netDisk_getNet(netDisk, netMisc_stringToName(netName));
	logInfo("Parsed the top level net of the cactus tree to check\n");

	///////////////////////////////////////////////////////////////////////////
	// Recursive check the nets.
	///////////////////////////////////////////////////////////////////////////

	int64_t startTime = time(NULL);
	FILE *fileHandle = fopen(outputFile, "w");
	makeMAFHeader(net, fileHandle);
	getMAFs(net, fileHandle);
	fclose(fileHandle);
	logInfo("Got the mafs in %i seconds/\n", time(NULL) - startTime);

	///////////////////////////////////////////////////////////////////////////
	// Clean up.
	///////////////////////////////////////////////////////////////////////////

	netDisk_destruct(netDisk);

	return 0;
}