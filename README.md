This tool is meant to extract textures from the Treyarch texture format (IPAK) for the Wii U version of Call of Duty: Black Ops 2. Sadly, the tool remains uncomplete since it needs [ninTexUtils](https://github.com/aboood40091/ninTexUtils) implementation to directly output the texture in a good way.

# About IPAK
IPAK used to be a proprietary file format used by Treyarch to store every texture asset from their games, IPAK contains different partitions and the textures are  compressed using LZO1X algorithm.
The texture format used are the following:
- DXT1
- DXT3
- DXT5
- A8R8G8B8

# How to use
`ipak_extractor.exe <input_file> <output_directory>`
