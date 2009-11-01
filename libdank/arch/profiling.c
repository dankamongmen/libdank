#include <libdank/ersatz/compat.h>
#include <libdank/arch/profiling.h>
#include <libdank/objects/logctx.h>
#include <libdank/objects/objustring.h>
#ifdef LIB_COMPAT_FREEBSD
#include <pmc.h>
#include <sys/module.h>

static struct pmcidctx {
	const char *evname;
	pmc_id_t pmcid;
	int enabled;
} tcpmcids[] = {
	{ .evname = "unhalted-cycles",	},
	{ .evname = NULL,		}
}, scpmcids[] = {
	{ .evname = NULL,		}
};

static int pmc_initialized;

static int
stringize_pmc_cpuinfo(ustring *u){
	const struct pmc_cpuinfo *cpuinfo_ptr;
	struct pmc_cpuinfo cpuinfo;

	if(pmc_cpuinfo(&cpuinfo_ptr) || !cpuinfo_ptr){
		moan("Couldn't get PMC CPU info\n");
		return -1;
	}
	cpuinfo = *cpuinfo_ptr;
#define PMC_CPUINFO_TAG "pmc_cpuinfo"
	if(printUString(u,"<"PMC_CPUINFO_TAG">") < 0){
		return -1;
	}
#define STRINGIZE_PMC_CPUINFO(field,str) \
	if(printUString(u,"<"#field">%s</"#field">",str) < 0){ return -1; }
	STRINGIZE_PMC_CPUINFO(cputype,pmc_name_of_cputype(cpuinfo.pm_cputype));
#undef STRINGIZE_PMC_CPUINFO
#define STRINGIZE_PMC_CPUINFO(field,val) \
	if(printUString(u,"<"#field">%d</"#field">",val) < 0){ return -1; }
	STRINGIZE_PMC_CPUINFO(ncpu,pmc_ncpu());
#undef STRINGIZE_PMC_CPUINFO
	if(printUString(u,"</"PMC_CPUINFO_TAG">") < 0){
		return -1;
	}
#undef PMC_CPUINFO_TAG
	return 0;
}

static int
stringize_pmc_driverstats(ustring *u){
	struct pmc_driverstats dstats;

	if(pmc_get_driver_stats(&dstats)){
		moan("Couldn't get PMC driver stats\n");
		return -1;
	}
#define PMC_DSTATS_TAG "pmc_driverstats"
	if(printUString(u,"<"PMC_DSTATS_TAG">") < 0){
		return -1;
	}
#define STRINGIZE_PMC_DRIVERSTAT(stat) \
	if(printUString(u,"<"#stat">%d</"#stat">",dstats.pm_##stat) < 0){ return -1; }
	STRINGIZE_PMC_DRIVERSTAT(intr_ignored);
	STRINGIZE_PMC_DRIVERSTAT(intr_processed);
	STRINGIZE_PMC_DRIVERSTAT(intr_bufferfull);
	STRINGIZE_PMC_DRIVERSTAT(syscalls);
	STRINGIZE_PMC_DRIVERSTAT(syscall_errors);
	STRINGIZE_PMC_DRIVERSTAT(buffer_requests);
	STRINGIZE_PMC_DRIVERSTAT(buffer_requests_failed);
	STRINGIZE_PMC_DRIVERSTAT(log_sweeps);
#undef STRINGIZE_PMC_DRIVERSTATS
	if(printUString(u,"</"PMC_DSTATS_TAG">") < 0){
		return -1;
	}
#undef PMC_DSTATS_TAG
	return 0;
}

static int
stringize_pmcid(ustring *u,const struct pmcidctx *p){
	unsigned msr;

#define PMCID_STR "pmcid"
	if(printUString(u,"<" PMCID_STR ">") < 0){
		return -1;
	}
	if(printUString(u,"<event>%s</event>",p->evname) < 0){
		return -1;
	}
	if(p->enabled){
		if(pmc_get_msr(p->pmcid,&msr)){
			bitch("Couldn't get MSR for %s\n",p->evname);
			return -1;
		}
		if(printUString(u,"<msr>%u</msr>",msr) < 0){
			return -1;
		}
	}
	if(printUString(u,"</" PMCID_STR ">") < 0){
		return -1;
	}
#undef PMCID_STR
	return 0;
}

static int
stringize_pmc(ustring *u){
	typeof (*tcpmcids) *cur;

	for(cur = tcpmcids ; cur->evname ; ++cur){
		if(stringize_pmcid(u,cur)){
			return -1;
		}
	}
	for(cur = scpmcids ; cur->evname ; ++cur){
		if(stringize_pmcid(u,cur)){
			return -1;
		}
	}
	if(stringize_pmc_driverstats(u)){
		return -1;
	}
	if(stringize_pmc_cpuinfo(u)){
		return -1;
	}
	return 0;
}

static int
check_hwpmc_module(void){
	int fid;
	
	if((fid = modfind("hwpmc")) < 0){
		return -1;
	}
	return 0;
}

int init_profiling(void){
	ustring u = USTRING_INITIALIZER;
	typeof (*tcpmcids) *cur;

	if(check_hwpmc_module()){
		bitch("The hwpmc module wasn't loaded\n");
		return -1;
	}
	if(pmc_init()){
		moan("Couldn't initialize the hwpmc system (no hardware support?)\n");
		return -1;
	}
	for(cur = tcpmcids ; cur->evname ; ++cur){
		if(pmc_allocate(cur->evname,PMC_MODE_TC,0,PMC_CPU_ANY,&cur->pmcid)){
			moan("Couldn't allocate a TC PMC for %s\n",cur->evname);
		}else if(pmc_start(cur->pmcid)){
			moan("Couldn't start a TC PMC for %s\n",cur->evname);
			pmc_release(cur->pmcid);
		}else{
			cur->enabled = 1;
		}
	}
	for(cur = scpmcids ; cur->evname ; ++cur){
		if(pmc_allocate(cur->evname,PMC_MODE_SC,0,PMC_CPU_ANY,&cur->pmcid)){
			moan("Couldn't allocate a SC PMC for %s\n",cur->evname);
		}else if(pmc_start(cur->pmcid)){
			moan("Couldn't start a SC PMC for %s\n",cur->evname);
			pmc_release(cur->pmcid);
		}else{
			cur->enabled = 1;
		}
	}
	nag("Initialized performance management counters\n");
	if(stringize_pmc(&u)){
		return -1;
	}
	nag("%s\n",u.string);
	reset_ustring(&u);
	pmc_initialized = 1;
	return 0;
}

int stop_profiling(void){
	ustring u = USTRING_INITIALIZER;
	typeof (*tcpmcids) *cur;
	int ret = 0;

	if(pmc_initialized){
		for(cur = tcpmcids ; cur->evname ; ++cur){
			if(cur->enabled){
				if(pmc_stop(cur->pmcid)){
					moan("Couldn't stop TC PMC for %s\n",cur->evname);
					ret = -1;
				}
				if(pmc_release(cur->pmcid)){
					moan("Couldn't release TC PMC for %s\n",cur->evname);
					ret = -1;
				}
				cur->enabled = 0;
			}
		}
		for(cur = scpmcids ; cur->evname ; ++cur){
			if(cur->enabled){
				if(pmc_stop(cur->pmcid)){
					moan("Couldn't stop SC PMC for %s\n",cur->evname);
					ret = -1;
				}
				if(pmc_release(cur->pmcid)){
					moan("Couldn't release SC PMC for %s\n",cur->evname);
					ret = -1;
				}
				cur->enabled = 0;
			}
		}
		if(stringize_pmc_driverstats(&u) || stringize_pmc_cpuinfo(&u)){
			ret = -1;
		}else{
			nag("%s\n",u.string);
		}
		reset_ustring(&u);
		pmc_initialized = 0;
	}
	return ret;
}
#else
int init_profiling(void){
	bitch("Profiling is not implemented on this OS\n");
	return -1;
}

int stop_profiling(void){
	return 0;
}
#endif
