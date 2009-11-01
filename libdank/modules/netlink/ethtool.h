#ifndef ARGUS_ETHTOOL
#define ARGUS_ETHTOOL

int check_ethtool_support(const char *,char **);
int get_nic_linkspeed(const char *);
int get_nic_linkstatus(const char *);

#endif
