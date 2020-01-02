#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
//****************************************
//      I implemented extra credit!
//****************************************

// read the assignment instructions about this!
#define MAX_REASONABLE_TEXT_CHUNK_SIZE 1024

// the PNG file signature.
const char PNG_SIGNATURE[8] = {137, 'P', 'N', 'G', '\r', '\n', 26, '\n'};

// see if two strings are equal.
bool streq(const char* a, const char* b) { return strcmp(a, b) == 0; }

// see if the first n characters of two strings are equal.
bool strneq(const char* a, const char* b, int n) { return strncmp(a, b, n) == 0; }

// given a chunk's identifier and a type, sees if they're equal. use like:
//    <read a chunk's length and identifier>
//    if(is_chunk(identifier, "IHDR"))
bool is_chunk(const char* identifier, const char* type) {
	return strneq(identifier, type, 4);
}

// byte-swaps a 32-bit integer.
unsigned int bswap32(unsigned int x) {
	return
		((x & 0xFF) << 24) |
		((x & 0xFF00) << 8) |
		((x & 0xFF0000) >> 8) |
		((x & 0xFF000000) >> 24);
}
//------------------------------------------------
//          *** MY WORK STARTS HERE ***
//------------------------------------------------

//Takes a filename, returns a FILE*
FILE* get_file(const char* filename){
	FILE* fp;
	fp = fopen(filename, "r+");
	if(!fp){ //check for failed opening
		printf("Error: unable to open file\n");
		exit(1);
	}
	char signature[10];
	fread(signature, 1, 8, fp); //read in file signature
	if(!strneq(signature, PNG_SIGNATURE, 8)){
		printf("Error: file is not a PNG\n");
		exit(1); //check that file is a PNG. exit otherwise
	}
	return fp;
}

//a struct to store a chunk header
typedef struct{
	unsigned int length;
	char identifier[4]; 
} Header;

//a struct to store file info
typedef struct{
	unsigned int width;
	unsigned int height;
	char bitDepth;
	char colorType;
	char compression;
	char filtering;
	char interlaced;
} FileInfo;

//reads header info into a struct
//use only after get_file
Header read_header(FILE* f){
	Header ret = {};
	fread(&ret.length, 4, 1, f);
	fread(ret.identifier, 1, 4, f);
	ret.length = bswap32(ret.length);
	return ret;
}

//reads png info into a struct
//use after read_header
FileInfo read_fileInfo(FILE* f){
	FileInfo ret = {};
	fread(&ret, 13, 1, f); //reads that shit
	ret.width = bswap32(ret.width); //flips them bits
	ret.height = bswap32(ret.height);
	return ret;
}

//A function that reads and prints text chunks
void print_text_chunk(FILE* f, unsigned int length){
	char text[length+1]; //holds values plus '\0'
	fread(&text, 1, length, f); //reads it in
	text[length] = '\0'; //null-terminates the string
	printf("%s:\n", text); //prints to first null
	//finds where the null break is, then prints value after
	int firstNull = 0;
	while(1){
		if(text[firstNull] == '\0'){
			break;
		}
		firstNull++;
	}
	printf("   %s\n", (text + firstNull + 1));
	//important: advances past CRC so pointer is at next chunk
	fseek(f, 4, SEEK_CUR);
}

//A function to move a file pointer to IEND
//used for the add_text function
void goto_iend(FILE* f){
	Header myHeader;
	while(1){
		myHeader = read_header(f);
		if(is_chunk(myHeader.identifier, "IEND")){
			//if it's an end chunk, rewind to beginning and break
			fseek(f, -8, SEEK_CUR);
			break;
		}
		//otherwise, go to next chunk
		fseek(f, (myHeader.length + 4), SEEK_CUR);
	}
}

// ------------------------------------------------------------------------------------------------
// Commands (this is what you'll implement!!!!)
// ------------------------------------------------------------------------------------------------

void show_info(const char* filename) {
	//open file, fill structs with info
	FILE* f = get_file(filename);
	Header myHeader = read_header(f);
	//i get an "unused variabe" warning without this. may fix later
	unsigned int stoptheerror = myHeader.length;
	stoptheerror++;
	FileInfo myFileInfo = read_fileInfo(f);
	//determine color type from the byte
	char colorType[20];
	switch((int)myFileInfo.colorType){
		case 0: strcpy(colorType, "Grayscale"); break;
		case 2: strcpy(colorType, "RGB");		break;
		case 3: strcpy(colorType, "Indexed"); 	break;
		case 4: strcpy(colorType, "Grayscale + Alpha"); break;
		case 6: strcpy(colorType, "RGB + Alpha"); break;
		default: printf("Can't touch this\n");
	}
	//print the info
	printf("File info:\n");
	printf("   Dimensions: %u x %u\n", myFileInfo.width, myFileInfo.height);
	printf("   Bit depth: %d\n", myFileInfo.bitDepth);
	printf("   Color type: %s\n", colorType);
	if(myFileInfo.interlaced){
		printf("   Interlaced: yes\n");
	}else{
		printf("   Interlaced: no\n");
	}
}

void dump_chunks(const char* filename) {
	//open file. this reads file sig and leaves us at IHDR
	FILE* f = get_file(filename);
	while(1){
		Header myHeader = read_header(f);
		//len 4b, id 4b, data len, crc 4. read_head puts out at data
		printf("%.4s (length = %d)\n", myHeader.identifier, myHeader.length);
		fseek(f, (myHeader.length + 4), SEEK_CUR);
		if(is_chunk(myHeader.identifier, "IEND")){
			break; //breaks at the end chunk
		}
	}
}

void show_text(const char* filename) {
	FILE* f = get_file(filename);
	while(1){
		Header myHeader = read_header(f);
		if(is_chunk(myHeader.identifier, "tEXt")){
			//if it's a text chunk, check length and print contents
			if(myHeader.length > MAX_REASONABLE_TEXT_CHUNK_SIZE){
				printf("Error: chunk too chunky\n");
			}else{
				print_text_chunk(f, myHeader.length);
			}
		}else{
			//otherwise fast forward and/or exit
			fseek(f, (myHeader.length + 4), SEEK_CUR);
			if(is_chunk(myHeader.identifier, "IEND")){
				break;
			}
		}
	}
}

void add_text(const char* filename, const char* textName, const char* textValue) {
	FILE* f = get_file(filename); //opens file
	goto_iend(f); //goes to first byte of IEND
	//okay, write time
	//first, write in length of new tEXt chunk
	//gotta find length first (add 1 for n.t.)
	unsigned int length =  strlen(textName) + strlen(textValue) + 1;
	length = bswap32(length); //big-endian!
	fwrite(&length, 4, 1, f);
	//write in new identifier
	char tEXt[4] = {'t', 'E', 'X', 't'}; 
	fwrite(&tEXt, 1, 4, f);
	//write in the text name, including '\0'
	fwrite(textName, 1, (strlen(textName) + 1), f);
	//write in the text value
	fwrite(textValue, 1, strlen(textValue), f);
	//write in crc
	char zeros[4] = {'\0', '\0', '\0', '\0'};
	fwrite(&zeros, 1, 4, f);
	//next, we write in a IEND chunk
	//length
	fwrite(&zeros, 1, 4, f);
	//identifier
	char IEND[4] = {'I', 'E', 'N', 'D'};
	fwrite(&IEND, 1, 4, f);
	//CRC
	fwrite(&zeros, 1, 4, f);
	//close file
	fclose(f);
}
// ------------------------------------------------------------------------------------------------
// Argument parsing
// ------------------------------------------------------------------------------------------------

typedef enum {
	Info,
	DumpChunks,
	Text,
	Add,
} Mode;

typedef struct {
	const char* input;
	Mode mode;
} Arguments;

void show_usage_and_exit(const char* reason) {
	if(reason) {
		printf("Error: %s\n", reason);
	}

	printf("Usage:\n");
	printf("  ./pngedit input.png [command]\n");
	printf("Commands:\n");
	printf("  (no command)         displays PNG file info.\n");
	printf("  dump                 dump all chunks in the file.\n");
	printf("  text                 show all text chunks in the file.\n");
	printf("  add name text        add a text chunk with specified name and text.\n");
	exit(1);
}

Arguments parse_arguments(int argc, const char** argv) {
	Arguments ret = {};

	switch(argc) {
		case 1: show_usage_and_exit(NULL);
		case 2: ret.mode = Info; break;
		case 3: {
			if(streq(argv[2], "dump")) {
				ret.mode = DumpChunks;
			} else if(streq(argv[2], "text")) {
				ret.mode = Text;
			} else {
				show_usage_and_exit("Invalid command.\n");
			}
			break;
		}
		case 5:{
			if(streq(argv[2], "add") && strlen(argv[3]) < 80){
				ret.mode = Add;
			}else{
				show_usage_and_exit("Invalid command or text name too long\n");
			}
			break;
		}
		default: show_usage_and_exit("Invalid arguments.");
	}

	// if we get here, argv[1] is valid.
	ret.input = argv[1];
	return ret;
}

int main(int argc, const char** argv) {
	Arguments args = parse_arguments(argc, argv);
	switch(args.mode) {
		case Info:       show_info(args.input);   break;
		case DumpChunks: dump_chunks(args.input); break;
		case Text:       show_text(args.input);   break;
		case Add:		 add_text(args.input, argv[3], argv[4]); break;
		default:
			printf("well this should never happen!\n");
			return 1;
	}

	return 0;
}
