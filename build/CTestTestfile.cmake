# CMake generated Testfile for 
# Source directory: /home/zxx/internet/net-lab
# Build directory: /home/zxx/internet/net-lab/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(eth_in "/home/zxx/internet/net-lab/build/eth_in" "/home/zxx/internet/net-lab/testing/data/eth_in")
set_tests_properties(eth_in PROPERTIES  _BACKTRACE_TRIPLES "/home/zxx/internet/net-lab/CMakeLists.txt;161;add_test;/home/zxx/internet/net-lab/CMakeLists.txt;0;")
add_test(eth_out "/home/zxx/internet/net-lab/build/eth_out" "/home/zxx/internet/net-lab/testing/data/eth_out")
set_tests_properties(eth_out PROPERTIES  _BACKTRACE_TRIPLES "/home/zxx/internet/net-lab/CMakeLists.txt;166;add_test;/home/zxx/internet/net-lab/CMakeLists.txt;0;")
add_test(arp_test "/home/zxx/internet/net-lab/build/arp_test" "/home/zxx/internet/net-lab/testing/data/arp_test")
set_tests_properties(arp_test PROPERTIES  _BACKTRACE_TRIPLES "/home/zxx/internet/net-lab/CMakeLists.txt;171;add_test;/home/zxx/internet/net-lab/CMakeLists.txt;0;")
add_test(ip_test "/home/zxx/internet/net-lab/build/ip_test" "/home/zxx/internet/net-lab/testing/data/ip_test")
set_tests_properties(ip_test PROPERTIES  _BACKTRACE_TRIPLES "/home/zxx/internet/net-lab/CMakeLists.txt;176;add_test;/home/zxx/internet/net-lab/CMakeLists.txt;0;")
add_test(ip_frag_test "/home/zxx/internet/net-lab/build/ip_frag_test" "/home/zxx/internet/net-lab/testing/data/ip_frag_test")
set_tests_properties(ip_frag_test PROPERTIES  _BACKTRACE_TRIPLES "/home/zxx/internet/net-lab/CMakeLists.txt;181;add_test;/home/zxx/internet/net-lab/CMakeLists.txt;0;")
add_test(icmp_test "/home/zxx/internet/net-lab/build/icmp_test" "/home/zxx/internet/net-lab/testing/data/icmp_test")
set_tests_properties(icmp_test PROPERTIES  _BACKTRACE_TRIPLES "/home/zxx/internet/net-lab/CMakeLists.txt;186;add_test;/home/zxx/internet/net-lab/CMakeLists.txt;0;")
add_test(udp_test "/home/zxx/internet/net-lab/build/udp_test" "/home/zxx/internet/net-lab/testing/data/udp_test")
set_tests_properties(udp_test PROPERTIES  _BACKTRACE_TRIPLES "/home/zxx/internet/net-lab/CMakeLists.txt;191;add_test;/home/zxx/internet/net-lab/CMakeLists.txt;0;")
add_test(tcp_test "/home/zxx/internet/net-lab/build/tcp_test" "/home/zxx/internet/net-lab/testing/data/tcp_test")
set_tests_properties(tcp_test PROPERTIES  _BACKTRACE_TRIPLES "/home/zxx/internet/net-lab/CMakeLists.txt;196;add_test;/home/zxx/internet/net-lab/CMakeLists.txt;0;")
