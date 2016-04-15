CXX=clang++
ROOT=../..
INCPATHS= -I$(ROOT)/src/block
LIBPATHS = -L${ROOT}/img
LINKLIBS = -lcflsh_block -lpthread -ludev
LINK_FLAGS= ${LIBPATHS} ${LINKLIBS}
CXX_FLAGS= -std=c++11 -Wall -Werror ${INCPATHS} ${LINK_FLAGS}

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${ROOT}/img

all:
	$(CXX) $(CXX_FLAGS) mykv.cc -o mykv
	$(CXX) $(CXX_FLAGS) mykv_client.cc -o mykv_client
