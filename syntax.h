
#define RESET             "\e[0m"
#define UNDERLINE         "\e[4m"
#define DEFAULT_F         "\e[39m"
#define DEFAULT_B         "\e[49m"

// Use 8-bit Colours format (bat only 4-bit used)
#define BLACK             "\e[38;5;000m"
#define MAROON            "\e[38;5;001m"
#define GREEN             "\e[38;5;002m"
#define OLIVE             "\e[38;5;003m"
#define NAVY              "\e[38;5;004m"
#define PURPLE            "\e[38;5;005m"
#define TEAL              "\e[38;5;006m"
#define SILVER            "\e[38;5;007m"
#define GREY              "\e[38;5;008m"
#define RED               "\e[38;5;009m"
#define LIME              "\e[38;5;010m"
#define YELLOW            "\e[38;5;011m"
#define BLUE              "\e[38;5;012m"
#define FUCHSIA           "\e[38;5;013m"
#define AQUA              "\e[38;5;014m"
#define WHITE             "\e[38;5;015m"


#define HL_CISCO          1
#define HL_COND           2
#define HL_KEYWORD        3
#define HL_PROTOCOL       4
#define HL_CONFIGURE      5
#define HL_FUNCTION       6
#define HL_COMMENT        7
#define HL_STRING         8
#define HL_INTERFACE      9
#define HL_ACTION         10
#define HL_VAR            11
#define HL_IPV4           12
#define HL_IPV6           13
#define HL_SPACE          14


//enum palette {
//  BLACK,
//  RED,
//  GREEN,
//  YELLOW,
//  BLUE,
//  MAGENTA,
//  CYAN,
//  WHITE,
//  COLOR_MAX
//};

//CISCO   = cisco
//COND    = match|eq|neq|gt|lt|ge|le|range
//KEYWORD = speed|duplex|negotiation|delay|bandwidth|preempt|priority|timers
//KEYWORD = logging|log|login|password|username|user|license|host|hostname|system
//KEYWORD = address|network|route|neighbor|redistribute|default-gateway|community
//KEYWORD = version|class|switchport|clock|name|minimum|maximum|level|size
//KEYWORD = established|source|destination|allowed
//KEYWORD = timeout|threshold|frequency|keepalive|average|weights|mtu|tunnel
//KEYWORD = privilege|secret

//PROTOCOL = ipv4 ipv6 tcp udp icmp echo
//PROTOCOL = http https www dhcp domain nameserver ssh telnet ftp ftp-data
//PROTOCOL = ntp snmp snmptrap syslog
//PROTOCOL = smtp pop2 pop3
//PROTOCOL = klogin kshell login rlogin sunrpc
//PROTOCOL = mpls rip isis ospf ospfv3 eigrp bgp hsrp vrrp ipsla
//PROTOCOL = isdn dial hdlc frame-relay atm
//PROTOCOL = igmp multicast broadcast
//PROTOCOL = rsa pki isakmp ipsec ike esp gre vpn mvpn pppoe
//PROTOCOL = qos cef pim ahp tacacs
//PROTOCOL = cdp lldp vtp spanning-tree lacp dot1q l2tun ethernet
//PROTOCOL = aaa aaa-server
//PROTOCOL = /traps\?/

//CONFIGURE = activate set default redundancy prefe ron tag
//CONFIGURE = inside outside input output static export import

//FUNCTION = service crypto encapsulation standby mode in out
//FUNCTION = storm-control snmp-server group nat banner mask seq metric
//FUNCTION = add-route shape rd route-target as-path remote-as
//FUNCTION = access-list access-class access-group prefix-list
//FUNCTION = passive-interface distribute-list permit subnet-zero
//FUNCTION = /channel\-\(group\|protocol\)/

//COMMENT = /!.*$/
//COMMENT = /no\s.*$/
//COMMENT = /description.*$/
//COMMENT = /remark.*$/
//COMMENT = /\s*#.*$/

//STRING = /\"[^"]*\"/

//INTERFACE = /^\(interface\|vlan\|line\|router\|track\)\s.*\d$/
//INTERFACE = /^ip\s\(sla\|vrf\)\s.*\d$/
//INTERFACE = /^monitor\ssession\s\d\+$/
//INTERFACE = /^\(class\|policy\|route\)\-map\s.*$/
//INTERFACE = /^ip\saccess\-list\s\(standard\|extended\)\s.*$/
//INTERFACE = /^vrf\s\(definition\|context\)\s.*$/
//INTERFACE = /^address\-family\sipv.*$/

//ACTION = disable deny shutdown down none

//VAR = trunk access full full-duplex auto active monitor
//VAR = any enable disable
//VAR = pvst mst rapid-pvst \transparent server client
//VAR = /\d\+[mg]\?/

//IPV4 = /\(25[0-5]\|2[0-4]\d\|[01]\?\d\{1,2}\)\(\.\(25[0-5]\|2[0-4]\d\|[01]\?\d\{1,2}\)\)\{3}\(\/[0-9]\{1,2\}\)\?/
