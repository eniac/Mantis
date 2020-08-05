# Example scripts for program independent set up

import argparse
import importlib
import json
import os
import signal
import sys
import time

SDE = os.getenv('SDE')

sys.path.append(SDE+"/install/lib/python2.7/site-packages/tofino")
import pal_rpc.pal as pal_i
from pal_rpc.ttypes import *
import conn_mgr_pd_rpc.conn_mgr as conn_mgr_i
from res_pd_rpc.ttypes import *
sys.path.append(SDE+"/install/lib/python2.7/site-packages")
# For type conversions
from ptf.thriftutils import *

from thrift.transport import TSocket
from thrift.transport import TTransport
from thrift.protocol import TBinaryProtocol
from thrift.protocol import TMultiplexedProtocol


transport = TSocket.TSocket('localhost', 9090)
transport = TTransport.TBufferedTransport(transport)
bprotocol = TBinaryProtocol.TBinaryProtocol(transport)
transport.open()

pal_protocol = TMultiplexedProtocol.TMultiplexedProtocol(bprotocol, "pal")
pal = pal_i.Client(pal_protocol)

try:

    # Example port configuration
    pal.pal_port_add(0, 144, pal_port_speed_t.BF_SPEED_10G, pal_fec_type_t.BF_FEC_TYP_NONE)
    pal.pal_port_enable(0, 144)

    pal.pal_port_add(0, 155, pal_port_speed_t.BF_SPEED_25G, pal_fec_type_t.BF_FEC_TYP_NONE)
    pal.pal_port_enable(0, 155)

    pal.pal_port_add(0, 153, pal_port_speed_t.BF_SPEED_10G, pal_fec_type_t.BF_FEC_TYP_NONE)
    pal.pal_port_enable(0, 153)    

    pal.pal_port_add(0, 161, pal_port_speed_t.BF_SPEED_10G, pal_fec_type_t.BF_FEC_TYP_NONE)
    pal.pal_port_enable(0, 161)    

    pal.pal_port_add(0, 163, pal_port_speed_t.BF_SPEED_10G, pal_fec_type_t.BF_FEC_TYP_NONE)
    pal.pal_port_enable(0, 163)          
    print("Port configuration suceeds")

except:
    print("Port configuration fails")