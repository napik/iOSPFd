cmake_minimum_required(VERSION 3.16.3)
project(iOSPFd)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS -pthread)

include_directories(src)

install(CODE "execute_process(
  			COMMAND
  				${SETCAP_EXECUTABLE}
  				cap_net_raw,cap_net_admin+ep
  				${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/dumpcap${CMAKE_EXECUTABLE_SUFFIX}
  			RESULT_VARIABLE
  				_SETCAP_RESULT
  			)
  			if( _SETCAP_RESULT )
  				message( WARNING \"setcap failed (${_SETCAP_RESULT}).\")
  			endif()"
        )

add_executable(iOSPFd
        src/fsmlist.h
        src/ospf.cpp
        src/ospf.hpp
        main.cpp)