#include <cunit/cunit.h>
#include <libdank/utils/string.h>
#include <libdank/utils/rfc2396.h>
#include <libdank/utils/memlimit.h>

// We ought to have these broken out as components, and put them together so as
// to properly test URI extraction FIXME
static const char *icaptext[] = {
	"icap://example.com",
	"icap://example.com/",
	"icap://example.com/index.html/erpo/index.html",
	"icap://root@localhost",
	"icap://example.net",
	"icap://icap.example.net/icap/example/",
	"icap://icap.example.com/",
	"icap://icap.example.com/ironmail?mode=compliance",
	"ICAP://example.net:1344",
	"ICAP://root@www.example.net:1344/",
	"ICAP://root@www.example.net:1344/index.cgi?query=icap;x=600;",
	"ICAP://ex%20ample.n%01et:1344",
	"ICAP://%41roo%66@www.example.net:1344/",
	"ICAP://root@www.example.net:1344/in%78ex.cgi?q%3fery=icap;x=6%300;",
	"icap://icap.example.com/iron,m$ail?mode=compliance",
	NULL
};

static const char *httptext[] = {
	"http://www.youtube.com/swf/l.swf?video_id=2GA3a15xF0c&rel=1&eurl=http%3A//lj-toys.com/%3Fjournalid%3D1438848%26moduleid%3D1%26preview%3D%26auth_token%3Dsessionless%3A1212408000%3Aembedcontent%3f&iurl=http%3A//s3.ytimg.com/vi/2GA3a15xF0c/default.jpg&t=OEgsToPDskLvjNzVtYSNvlMNseyE4ah2&hl=en",
	"http://ads.tw.adsonar.com/adserving/getAds.jsp?previousPlacementIds=1348086&placementId=1291088&pid=757767&ps=-1&zw=457&zh=215&url=http%3A//www.people.com/people/article/0%2C%2C20218095%2C00.html&v=5&dct=Spectacle%20%u2013%20and%20Tragedy%20%u2013-08%2C%20Yao%20Ming%20%3A%20People.com&ref=http%3A//www.people.com/people/0%2C%2C%2C00.html",
	"http:/this/is/a/relative/uri",
	"http:/this/is/a/relative/uri%20with/a/space",
	"http://userid:password@www.anywhere.com/",
	"http:thisisanopaqueuri",
	NULL
};

static const char *rfc2732text[] = {
	"http://[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:80/index.html",
	"http://[1080:0:0:0:8:800:200C:417A]/index.html",
	"http://[3ffe:2a00:100:7031::1]",
	"http://[1080::8:800:200C:417A]/foo",
	"http://[::192.9.5.5]/ipng",
	"http://[::FFFF:129.144.52.38]:80/index.html",
	"http://[2010:836B:4179::836B:4179]",
	NULL
};

static int
test_uris(const char **uris,const char *scheme){
	const char **cur;

	for(cur = uris ; *cur ; ++cur){
		char *cdup = Strdup(*cur),*ddup = Strdup(*cur);
		ustring us = USTRING_INITIALIZER;
		char *c = cdup,*d = ddup;
		uri *u;

		if(cdup == NULL || ddup == NULL){
			goto err;
		}
		printf(" URI: %s\n",c);
		printf("  (parsing test)...");
		if(parse_uri(scheme,&c)){
			printf("Failure!\n");
			fprintf(stderr," Failed to parse %s.\n",*cur);
			goto err;
		}
		if(*c){
			printf("Failure!\n");
			fprintf(stderr," Failed to completely parse %s (left %s).\n",*cur,c);
			goto err;
		}
		printf("success.\n");
		printf("  (extraction test)...");
		if((u = extract_uri(scheme,&d)) == NULL){
			printf("Failure!\n");
			fprintf(stderr," Failed to extract %s.\n",*cur);
			goto err;
		}
		if(*d){
			printf("Failure!\n");
			fprintf(stderr," Failed to completely parse %s (left %s).\n",*cur,d);
			free_uri(&u);
			goto err;
		}
		printf("success.\n");
		if(u->host) { printf("    host: %s\n",u->host); }
		if(u->userinfo) { printf("    user: %s\n",u->userinfo); }
		if(u->port) { printf("    port: %hu\n",u->port); }
		if(u->path) { printf("    path: %s\n",u->path); }
		if(u->query) { printf("    query: %s\n",u->query); }
		if(u->fragment) { printf("    fragment: %s\n",u->fragment); }
		if(u->opaque_part) { printf("    opaque: %s\n",u->opaque_part); }
		printf("  (stringize test)...");
		if(stringize_uri(&us,u) < 0){
			free_uri(&u);
			goto err;
		}
		printf("%s\n",us.string);
		if(strcmp(us.string,*cur)){
			fprintf(stderr," Failed to stringize %s correctly.\n",*cur);
			reset_ustring(&us);
			free_uri(&u);
			goto err;
		}
		reset_ustring(&us);
		printf("  (cleanup test)...");
		free_uri(&u);
		printf("success.\n");
		Free(cdup);
		Free(ddup);
		continue;

err:
		Free(cdup);
		Free(ddup);
		return -1;
	}
	return 0;
}

static int
test_httpuri(void){
	return test_uris(httptext,"http");
}

static int
test_icapuri(void){
	return test_uris(icaptext,"icap");
}

static int
test_rfc2732uri(void){
	return test_uris(rfc2732text,"http");
}

const declared_test RFC2396_TESTS[] = {
	{	.name = "icapuri",
		.testfxn = test_icapuri,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "httpuri",
		.testfxn = test_httpuri,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "rfc2732uri",
		.testfxn = test_rfc2732uri,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
