#include <iostream>
#include <fstream>
#include <string> 
#include <filesystem>
#include <vector>
#include <chrono>
#include <thread>
#include "ipak.h"
#include "lzo/lzo1x.h"

using namespace std;

// 32 bits per 32 bits
void swap_structure_endianness(uint32_t* s, size_t size) {
	for (uint32_t i = 0; i < size / 4; i++) {
		s[i] = _byteswap_ulong(s[i]);
	}
}

char *input_path;
char *output_path;

void collectChunkInfos(uint32_t meta_section_offset, vector<IPAKDataChunkMetaData>* meta_output) {
	ifstream infos_stream(input_path, ifstream::binary);
	if (infos_stream.fail()) {
		cerr << "IFS Infos Error: " << strerror(errno);
		exit(1);
	}
	infos_stream.seekg(meta_section_offset);
	string line;
	int idx = 0;
	char idx_name[100];

	// Parse infos 
	while (infos_stream.peek() != 0xA7) {
		IPAKDataChunkMetaData meta;

		if (infos_stream.tellg() == 0xffffffffffffffff) {
			cout << "aie";
		}

		// Get key
		infos_stream.read((char*)&meta.key, sizeof(uint64_t));
		meta.key = _byteswap_uint64(meta.key);

		// Skip random bits
		infos_stream.ignore(0x8);

		getline(infos_stream, line);	// name
		cout << line << endl;
		
		//strcpy((char*)&infos.name, &line.c_str()[12]);
		strcpy(idx_name, (to_string(idx) + "_").c_str());
		strcat(idx_name, &line.c_str()[12]);
		strcpy((char*)&meta.name, idx_name);

		getline(infos_stream, line);	// format
		//cout << line << endl;
		if (strcmp(&line.c_str()[8], "DXT1") == 0) {
			meta.format = FORMAT_DXT1;
		}
		else if (strcmp(&line.c_str()[8], "DXT3") == 0) {
			meta.format = FORMAT_DXT3;
		}
		else if (strcmp(&line.c_str()[8], "DXT5") == 0) {
			meta.format = FORMAT_DXT5;
		}
		else if (strcmp(&line.c_str()[8], "A8R8G8B8") == 0) {
			meta.format = FORMAT_A8R8G8B8;
		}

		getline(infos_stream, line);	// offset
		meta.offset = atoi(&line.c_str()[7]);

		getline(infos_stream, line);	// size
		//cout << line << endl;
		meta.size = atoi(&line.c_str()[5]);

		getline(infos_stream, line);	// width
		//cout << line << endl;
		meta.width = atoi(&line.c_str()[6]);

		getline(infos_stream, line);	// height
		//cout << line << endl;
		meta.height = atoi(&line.c_str()[7]);

		getline(infos_stream, line);	// levels
		meta.levels = atoi(&line.c_str()[7]);

		getline(infos_stream, line);	// mip
		meta.mip = atoi(&line.c_str()[4]);

		infos_stream.ignore(9);		// manual

		meta_output->push_back(meta);
		idx++;
	}
}

void collect_entries(uint32_t entry_section_offset, uint32_t entry_count, vector<IPakIndexEntry>* entries_output) {

	ifstream entries_stream(input_path, ifstream::binary);
	if (entries_stream.fail()) {
		cerr << "IFS Entries Error: " << strerror(errno);
		exit(1);
	}
	entries_stream.seekg(entry_section_offset);

	for (int i = 0; i < entry_count; i++) {
		IPakIndexEntry entry;

		entries_stream.read((char*)&entry, sizeof(IPakIndexEntry));

		entry.key = _byteswap_uint64(entry.key);
		entry.offset = _byteswap_ulong(entry.offset);
		entry.size = _byteswap_ulong(entry.size);


		entries_output->push_back(entry);
	}
}

int main(int argc, char** argv) {
	IPakHeader fheader;
	vector<IPAKDataChunkMetaData>* metas = new vector<IPAKDataChunkMetaData>();
	vector<IPakIndexEntry>* entries = new vector<IPakIndexEntry>();

	if (argc < 3) {
		cerr << "Usage: ipak_extractor.exe <input_file> <output_directory>";
		exit(1);
	}

	input_path = argv[1];
	output_path = argv[2];

	cout << "INPUT " << argv[1] << " OUTPUT " << argv[2];
	ifstream ifs(argv[1], ifstream::binary);

	if (ifs.fail()) {
		cerr << "IFS Error: " << strerror(errno);
		exit(1);
	}

	ifs.read((char*)&fheader, sizeof(IPakHeader));

	swap_structure_endianness((uint32_t*)&fheader, sizeof(IPakSection));


	// Check header magic
	if (fheader.magic == 0x4950414B) {	// 'IPAK'
		if (fheader.version == 0x00050000) {
			cout << fheader.sectionCount << " sections found\n";

			IPakSection* sections = new IPakSection[fheader.sectionCount];

			// Store section headers
			for (unsigned int i = 0; i < fheader.sectionCount; i++) {
				ifs.read((char*)&sections[i], sizeof(IPakSection));

				swap_structure_endianness((uint32_t*)&sections[i], sizeof(IPakSection));

				if (sections[i].type == IPAK_SECTION_METADATA) {
					collectChunkInfos(sections[i].offset, metas);
				}

				if (sections[i].type == IPAK_SECTION_ENTRY) {
					collect_entries(sections[i].offset, sections[i].itemCount, entries);
				}
			}

			for (unsigned int i = 0; i < fheader.sectionCount; i++) {
				cout << hex << "Section " << i << " type " << sections[i].type << " offset " << sections[i].offset << " size " << sections[i].size << " item_count " << sections[i].itemCount << endl;

				// Extract data section only for now
				if (sections[i].type != IPAK_SECTION_DATA) {
					continue;
				}

				// Skip endsection padding
				char cA7;
				int a7_count = 0;

				cA7 = ifs.peek();
				if (cA7 == '§') {
					do {
						ifs.get(cA7);
						cA7 = ifs.peek();
						a7_count++;
					} while (cA7 == '§');
				}

				int j = 0;
				// Iterating on chunks
				for (IPakIndexEntry entry : *entries) {
					int block_count;

					IPakDataChunkHeader chunkHeader;
					lzo_uint decomp_size = 0x200000;
					uint32_t compressed_data_size = 0;

					ifs.seekg(sections[i].offset + entry.offset);

					ifs.read((char*)&chunkHeader, sizeof(IPakDataChunkHeader));
					
					swap_structure_endianness((uint32_t*)&chunkHeader, sizeof(chunkHeader));
					block_count = (chunkHeader.countAndOffset & 0xFF000000) >> 24;

					//string output_name = to_string(i) + "_" + to_string(j);
					string output_name;
					for (IPAKDataChunkMetaData meta : *metas) {
						if (entry.key == meta.key) {
							output_name = meta.name;
							break;
						}
					}

					if (output_name.empty()) {
						cout << "Could not find a matching key" << endl;
						output_name = to_string(j);
					}
					j++;
					// Find the matching meta datas


					cout << "[" << output_name << "]" << endl;
					ofstream ofs(output_path + output_name, ofstream::binary);
					if (ofs.fail()) {
						cerr << "OFS Error: " << strerror(errno);
						exit(1);
					}
					bool need_decompression;
					// Big textures are cropped in different blocks
					for (int k = 0; k < block_count; k++) {
						//cout << "position before data block " << ifs.tellg() << endl;
						compressed_data_size = (chunkHeader.commands[k] & 0xFFFFFF);
						need_decompression = (chunkHeader.commands[k] & 0xFF000000) >> 24;
						if (compressed_data_size > 0x7FF0) {
							cout << "aie";
						}
						//cout << compressed_data_size << "    " << metas->at(j).size << endl;
						//cout << "compressed data size " << compressed_data_size << endl;
						char* compressed_chunk = new char[compressed_data_size];
						ifs.read((char*)compressed_chunk, compressed_data_size);
						char* decompressed_chunk = new char[0x200000];
						
						// Decompression
						if (need_decompression) {
							//cout << "Decompressing block " << j << "_" << k << " size " << compressed_data_size << endl;
							int r = lzo1x_decompress_safe((unsigned char*)compressed_chunk, (lzo_uint)compressed_data_size, (unsigned char*)decompressed_chunk, &decomp_size, NULL);
							
							if (r == LZO_E_OK)
								cout << "successfully decompressed\n";
							else
							{
								cerr << "decomp erro: " << r << endl;
								return 1;
							}
							//cout << "decomp size " << decomp_size << endl;
							ofs.write(decompressed_chunk, decomp_size);

							delete[] decompressed_chunk;
						}
						else {
							ofs.write(compressed_chunk, compressed_data_size);
						}
						delete[] compressed_chunk;
						//ofs.write(decompressed_chunk, decomp_size);


					}

					char c93;
					c93 = ifs.peek();
					int c93_counter = 0;
					if (c93 == '“') {
						do {
							ifs.get(c93);
							c93 = ifs.peek();
							c93_counter++;
						} while (c93 == '“');
					}
				}
			}
		}

		else {
			cerr << "Incorrect IPAK Version";
			exit(1);
		}
	}

	else {
		cerr << "File is not a wiiu IPAK";
		exit(1);
	}
}