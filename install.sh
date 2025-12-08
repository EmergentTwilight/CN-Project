wget https://www.nsnam.org/releases/ns-3.46.1.tar.bz2
tar xjf ns-3.46.1.tar.bz2
cd ns-3.46.1

# use system python/python3
pip install cppyy pybind11

# install brite
git clone https://gitlab.com/nsnam/BRITE.git
cd BRITE
mkdir build && cd build
cmake ..
make
cd ../..

# configure ns3
./ns3 configure --enable-examples --enable-tests --enable-python --with-brite=./BRITE
# 通过任何方式解决 configure 中的其他问题，直至至少满足这些条件：
# -- ---- Summary of ns-3 settings:
# Build profile                 : default
# Build directory               : /home/travis/course/CN/ns-3.46.1/build
# Build with runtime asserts    : ON
# Build with runtime logging    : ON
# Build version embedding       : OFF (not requested)
# BRITE Integration             : ON
# DES Metrics event collection  : OFF (not requested)
# DPDK NetDevice                : OFF (not requested)
# Emulation FdNetDevice         : ON
# Examples                      : ON
# File descriptor NetDevice     : ON
# GNU Scientific Library (GSL)  : ON
# GtkConfigStore                : ON
# LibXml2 support               : ON
# MPI Support                   : OFF (not requested)
# ns-3 Click Integration        : OFF (Missing headers: "simclick.h" and missing libraries: "nsclick click")
# ns-3 OpenFlow Integration     : OFF (Missing headers: "openflow.h" and missing libraries: "openflow")
# Netmap emulation FdNetDevice  : OFF (missing dependency)
# PyViz visualizer              : ON
# Python Bindings               : ON
# SQLite support                : ON
# Eigen3 support                : ON
# Tap Bridge                    : ON
# Tap FdNetDevice               : ON
# Tests                         : ON


# Modules configured to be built:
# antenna                   aodv                      applications              
# bridge                    brite                     buildings                 
# config-store              core                      csma                      
# csma-layout               dsdv                      dsr                       
# energy                    fd-net-device             flow-monitor              
# internet                  internet-apps             lr-wpan                   
# lte                       mesh                      mobility                  
# netanim                   network                   nix-vector-routing        
# olsr                      point-to-point            point-to-point-layout     
# propagation               sixlowpan                 spectrum                  
# stats                     tap-bridge                test                      
# topology-read             traffic-control           uan                       
# virtual-net-device        visualizer                wifi                      
# zigbee                    

# Modules that cannot be built:
# click                     mpi                       openflow 

# build ns3
./ns3 build  # takes a while
./test.py
# 如果只有 example failed，可以忽略

cd ..
make setup  # 备份 scratch 文件夹