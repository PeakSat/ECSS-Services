# ECSS-E-ST-70-41C Services

In this repository you can find the implementation of the ECSS services, based
on
the [ECSS-E-ST-70-41C](https://ecss.nl/standard/ecss-e-st-70-41c-space-engineering-telemetry-and-telecommand-packet-utilization-15-april-2016/)
standard.

## Documentation

You can read the complete documentation of this project at https://acubesat.gitlab.io/obc/ecss-services/docs/.
This includes installation instructions, a usage guide, and detailed descriptions of all public functions and
definitions.

## Directories

- **docs**: Source code usage and development documentation
- **inc**: All headers and libraries used in the code
- **src**: All source files are included here
- **test**: Unit test implementation

## Implementation status

| Service Type | Service Name                   | Implementation   |
|--------------|--------------------------------|------------------|
| ST[01]       | Request Verification           | Full             |
| ST[03]       | Housekeeping                   | Partial          |
| ST[04]       | Parameter Statistics Reporting | Partial          |
| ST[05]       | Event Reporting                | Partial          |
| ST[06]       | Memory Management              | Partial          |
| ST[08]       | Function Management            | Full             |
| ST[11]       | Time-based Scheduling          | Partial          |
| ST[12]       | On-board Monitoring            | Work in progress |
| ST[13]       | Large Packer Transfer          | Partial          |
| ST[15]       | On-board Storage and Retrieval | Work in progress |
| ST[17]       | Test                           | Partial          |
| ST[19]       | Event-action                   | Partial          |
| ST[20]       | Parameter Management           | Partial          |
| ST[23]       | File Management                | Work in progress |

## Build

The `ecss-services` repository can be built and tested without any access to specialised hardware. We provide build
instructions for Linux (or WSL) command-line, and the CLion IDE.

The dependencies of this repository are managed through the [conan](https://conan.io/) package manager, with repositories
from [ConanCenter](https://conan.io/center/) and [SpaceDot's packages](https://artifactory.spacedot.gr).

For more detailed installation instructions, including how to integrate with a microcontroller, visit the
[corresponding documentation page](https://acubesat.gitlab.io/obc/ecss-services/docs/md_docs_installation.html).

### From the Command Line (CLI)

1. Install a modern C++ compiler, CMake, and Conan.  
   CMake >= 3.23 and Conan >= 2.0 are recommended.
   <details>
   <summary>Getting conan</summary>

   You can install [conan](https://conan.io/) following the instructions [from here](https://docs.conan.io/2/installation.html).
   </details>
2. Clone the repository and enter the directory:
   ```shell
   git clone https://gitlab.com/acubesat/obc/ecss-services.git
   cd ecss-services
   ```
3. (If you haven't already) create a conan profile for your system:
   ```shell
   conan profile detect
   ```
4. (If you haven't already) add the SpaceDot repository to conan:
   ```shell
   conan remote add spacedot https://artifactory.spacedot.gr/artifactory/api/conan/conan
   ```
5. Download all dependencies and build the project through conan:
   ```shell
   conan build . --output-folder=build --build=missing --update --settings=build_type=Debug
   ```
6. Run the tests or the produced executable:
   ```shell
   build/Debug/tests
   build/Debug/x86_services
   ```
