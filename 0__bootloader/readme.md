# Cortex-M3 Bootloader 

The bootloader sample will be loaded by the Program ROM and its function is to
load application files. Some of the use cases supported are:

* Loading and running an application file.
* Loading one or multiple data files and an application file
* Loading an application file for each core.

The bootloader supports encryption using the key stored in the external
non-volatile memory (NVM) device. All elements can be individually encrypted.
The bootloader file format also offers data verification for all elements
using CCITT CRC. Encryption and data verification together also provide data
authentication.

## Building

To build the Arm Cortex-M3 bootloader, set the 'Release' build configuration as
the active configuration and build the project.

The project has the following build configurations:

- Release: Optimized for size. Uses the SDK libraries which are optimized for
  size.
- Debug: No optimizations. Uses nvmlib and fatfs libraries as workspace
  libraries. Fallback to the SDK libraries if the workspace libraries are not
  present.

## Bootloader API 

The user has three basic functions to choose from:

* bootloader_main
    * Reads the application file specified in the BOOT_APP_LOAD and 
BOOT_APP_EXT fields from the NVM device. Does not return.

* bootloader_boot 
    * Reads the application file specified in the BOOT_APP_LOAD and 
BOOT_APP_EXT fields from the NVM device. It might return if 
the Cortex-M3 core is not booted.

* bootloader_load_app_file 
    * Reads the application file specified in the argument. It might return 
if the Cortex-M3 core is not booted.

The bootloader supports two file types: application files and manifest files. 
Application files contain data and boot instructions, while manifest files 
have a list of application files to be loaded.


## Application Files

The most common output format for the Ezairo 8300 SDK toolchains is the ELF 
format. This format, however, does not provide data verification or 
security facilities and has only one entry point. The Ezairo 8300 application
files were designed to provide the necessary functionality and flexibility 
while offering a straightforward mapping between ELF files and application 
files.

Application files contain binary data and instructions on how to boot the 
cores and are composed of a **file header** and zero or more **data sections**.
Data sections can be created directly from program headers in ELF files.

The bootloader will start the loading process of an application file file 
reading the file header and verifying it. If the verification fails, the 
bootloader will stop the loading process. There is no rollback mechanism 
provided to clear any memory regions that have been loaded. Keep in mind that 
the ROM will clear the memory when the device is reset.

### Data Sections in Application Files

The data sections are loaded in the same order as they appear in the 
application file. After all the data sections have been loaded, the CFX core 
is booted and lastly the Cortex-M3 core is booted.

The CRC information for each data section is store in the file header. If the 
file header is encrypted and a data section is tampered with, the CRC 
verification for that data section will fail. It is recommended that the file 
header is always encrypted, even if the following data sections are not.

Several data section types are supported:

| Data Section Type | Description                                           | Memory Region Range                                       |
|:------------------|:-----------------------------------------------------:|:---------------------------------------------------------:|
| SEC_CFX_X         | Data destination is X memory                          | CFX_XRAM0_LSB_UNSIGNED_BASE : CFX_XRAM3_LSB_UNSIGNED_TOP  |
| SEC_CFX_Y         | Data destination is Y memory                          | CFX_YRAM0_LSB_UNSIGNED_BASE : CFX_YRAM2_LSB_UNSIGNED_TOP  | 
| SEC_CFX_P         | Data destination is the CFX core P memory             | CFX_PRAM_BASE : CFX_PRAM_TOP                              |
| SEC_CM3_P         | Data destination is the Arm Cortex-M3 core P memory   | PRAM_BASE : PRAM_TOP                                      |
| SEC_IOMEM         | Data destination is IO memory                         | No verification is performed                              |
| SEC_BOOT_CFX      | CFX core boot section                                 | -                                                         |
| SEC_BOOT_CM3      | CM3 core boot section                                 | -                                                         |

FENG and LPDSP32 data are loaded as SEC_IOMEM data sections.

The data sections in an application file can be individually encrypted. 
Encryption is applied to both the section header and the section data.

#### Boot Sections in Application Files

The boot sections are a special case of the data sections in that their data
layout is fixed. The data layout for boot sections is explained in the Boot
Section Layout section. Boot sections provide entry points for booting the
cores present in Ezairo 8300.

#### Volatile Sections

A volatile section will have its CRC verified, but if the verification fails 
the loading process will not be stopped.

## Initialization of Global Variables

The different program headers from the ELF file that have been generated by the build
tools are converted into data sections in the application files. This includes 
the .data section of the ELF file which holds the initialization data for the 
global variables.

The default startup routine for both cores does not explicitly initialize the 
.data section in memory. This is done by the bootloader when it loads the 
section in the application file that corresponds to the .data section of the 
ELF file.

If the initialization needs to be performed again, the .data section can be 
moved to a separate application file that contains just this section. The 
entire application can be loaded using a manifest file.

## Manifest Files

Manifest files provide a mechanism for supporting the case that a user 
application has been divided into more than one application file.

Manifest files contain a list of application files to be loaded. The 
application files will be loaded in the same order as they appear in the 
manifest file. Manifest files have a regular file header that is is followed 
by a list of file names in plain ASCII. The separator between file names is 
the CR character ('\n'). The file list must be null terminated. Manifest 
files can be encrypted.

A tipical use case for manifest files is to load a shared filter coefficient 
file and then an application file that uses the data previously loaded.

## Bootloader File Layout

### File Header

The file header is common to both application and manifest files. The 
following structure shows the layout of the file header:

```c
struct application_file_header
{
    uint32_t    ident;
    uint16_t    crc_table_size;
    uint16_t    crc_table[];
};
```

The `crc_table` should be followed by a padding of a single `uint16_t` word 
with `0x00` as its value if the number of entries in `crc_table` is **odd**. 
If the number of entries is **even** then there must not be any padding. The 
`crc_table_size` has the number of **valid** entries in the table, the 
padding should not be counted as an entry. The CRC information in a single 
`uint16_t` word is placed after the padding. The CRC is CCITT CRC stored in 
big-endian order.

The `ident` member represents a magic number that identifies the file type 
and can have the following values:

| File Type                       | Magic Number | Description                      |
|:--------------------------------|:------------:|:--------------------------------:|
| APPFILE_IDENT_APP               | 0x49415A45   | Application file (not encrypted) |
| APPFILE_IDENT_APPENCRYPTED      | 0x4B415A45   | Application file (encrypted)     |
| APPFILE_IDENT_MANIFEST          | 0x4D415A45   | Manifest file (not encrypted)    |
| APPFILE_IDENT_MANIFESTENCRYPTED | 0x4F415A45   | Manifest file (encrypted)        |

The maximum supported size for this structure is configured by the
APP_HEADER_MAXIMUM_SIZE preprocessor symbol. The default value is 512 bytes.

For files of the `APPFILE_IDENT_APPENCRYPTED` and 
`APPFILE_IDENT_MANIFESTENCRYPTED` types, the total size in bytes of the file 
header, not counting the `ident` field, must be a multiple of an AES block 
size (128 bits or 16 bytes). If the size is not a multiple of 16 bytes, then 
the appropriate number of padding bytes must be added **after** the CRC word. 
The values of these padding bytes is not important. They are not part of the 
CRC calculation. All data after the `ident` must be encrypted, while `ident` 
**should not** be encrypted.

### Data Section Layout

A data section begins with the a data_section_header:

```c
struct data_section_header
{
    uint32_t    offset;
    uint32_t    size;
    uint16_t    type;
    uint16_t    flags;
    uint16_t    _reserved;
    uint16_t    header_crc;
    uint32_t    data[];
};
```

The `header_crc` members is the CCITT CRC of the whole header, including the 
reserved area. The data CRC must be stored in big-endian order.

The `offset` holds the number of words between the beginning of the memory
section area shown in the Data Sections in Application Files section and the
destination address. as an example, if the destination address is
`CFX_XRAM0_LSB_UNSIGNED_BASE`, then `offset` should be `0x00000000`.

The possible values for the `flags` member are:

| File Type               | Value | Description                                         |
|:------------------------|:-----:|:---------------------------------------------------:|
| APPDATA_FLAGS_CLEARTEXT | 0x00  | Section data is not encrypted                       |
| APPDATA_FLAGS_ENCRYPTED | 0x80  | Section data is encrypted with the Application Key  |
| APPDATA_FLAGS_CRC       | 0x00  | CRC will be verified for the section                |
| APPDATA_FLAGS_VOLATILE  | 0x40  | CRC will not be verified (volatile section)         |

When a section is encrypted, it should have the `APPDATA_FLAGS_ENCRYPTED` 
value set. The `data_section_header` structure should be encrypted as 
successive AES blocks, the first block being the all elements up to and 
including `header_crc`. For an encrypted section, the data size must be a 
multiple of an AES block (16 bytes). If padding is used, it is considered 
part of the data and the corresponding CRC in the file header should include 
the padding bytes.

#### Boot Section Layout

A boot section has a regular `data_section_header` header, but a fixed data 
layout:

```c
struct boot_section_data
{
    uint32_t    stack_pointer;
    uint32_t    init_pointer;
};
```

The `init_pointer` hold the address that will be set in the Program Counter. 
The `stack_pointer` member holds the Stack Pointer address of the application 
and is only used by Cortex-M3 applications.

### Manifest File Layout

The manifest file differs from an application file as it does not have data 
sections, but a list of file names in pure ASCII. The file header will hold 
only one entry in the CRC table. This will be verified when the manifest file 
is loaded. In the case of failure the loading process is aborted.

The maximum supported size for a manifest file, including the file header, is
configured by the MANIFEST_MAXIMUM_SIZE preprocessor symbol. The default value
is 512 bytes.

If encryption is used, the padding requirement applies: the total size of the 
data part must be a multiple of 16, including the null termination.

## Creating the application data file from minimim_cm3

Add this pre-build step ot the Integration build configuration:

arm-none-eabi-objcopy -j .intvec -j .init -j .fini --strip-all --rename-section .intvec=.cm3_intvec --rename-section .init=.cm3_init --rename-section .fini=.cm3_fini ${workspace_loc:/minimum_cm3/Debug/minimum_cm3.elf} min.o
 
