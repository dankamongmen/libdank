#include <libdank/utils/fds.h>
#include <cunit/cunit.h>
#include <libdank/utils/syswrap.h>
#include <libxml/parser.h>
#include <libdank/modules/fileconf/sbox.h>

static FILE *
write_xml_file(const char *xmldata){
	FILE *fp;

	if( (fp = Tmpfile()) ){
		int fd = fileno(fp);

		if(Writen(fd,xmldata,strlen(xmldata))){
			Fclose(fp);
			fp = NULL;
		}else{
			rewind(fp);
		}
	}
	return fp;
}

static int
test_xmlaccept_main(void){
	static const char *XML_GOOD_DATA[] = {
		"<xmlparsetest/>",
		"<xmlparsetest></xmlparsetest>",
		"<xmlparsetest> <xmlstring>string</xmlstring>  </xmlparsetest>",
		"<xmlparsetest> <xmlstring> string </xmlstring>  </xmlparsetest>",
		"<xmlparsetest><xmlkleene/></xmlparsetest>",
		"<xmlparsetest><xmlkleene></xmlkleene></xmlparsetest>",
		"<xmlparsetest><xmlkleene/><xmlkleene/></xmlparsetest>",
		"<xmlparsetest><xmlkleene></xmlkleene><xmlkleene/></xmlparsetest>",
		"<xmlparsetest> <xmlkleene/> </xmlparsetest>",
		"<xmlparsetest> -=- <xmlkleene/> </xmlparsetest>",
		"<xmlparsetest> <xmlkleene/> //// </xmlparsetest>",
		"<xmlparsetest> -=- <xmlkleene/> //// </xmlparsetest>",
		"<xmlparsetest><xmlkleene><xmlkleene/></xmlkleene></xmlparsetest>",
		"<xmlparsetest><xmlu32> 0 </xmlu32></xmlparsetest>",
		"<xmlparsetest><xmlu32> 1 </xmlu32></xmlparsetest>",
		"<xmlparsetest><xmlu32>4294967295</xmlu32></xmlparsetest>",
		"<xmlparsetest><xmlu32> 4294967295</xmlu32></xmlparsetest>",
		"<xmlparsetest><xmlu32>4294967295 </xmlu32></xmlparsetest>",
		"<xmlparsetest><xmlu32> 4294967295 </xmlu32></xmlparsetest>",
		NULL
	};
	const char **data;

	xmlSetGenericErrorFunc(stdout,NULL);
	for(data = XML_GOOD_DATA ; *data ; ++data){
		xmlDocPtr xd;
		FILE *fp;

		if((fp = write_xml_file(*data)) == NULL){
			return -1;
		}
		if(reparse_xmlconf(&xd,fp)){
			return -1;
		}
		xmlFreeDoc(xd);
	}
	return 0;
}

static int
test_xmlreject_main(void){
	static const char *XML_BAD_DATA[] = {
		"", "   ", "<", ">", "<>", "</>",
		"</xmlparsetest>", "</xmlparsetest/>", "<xmlparsetes/t>",
		"<xmlparsetest\\>",
		NULL
	};
	const char **data;

	xmlSetGenericErrorFunc(stdout,NULL);
	for(data = XML_BAD_DATA ; *data ; ++data){
		xmlDocPtr xd;
		FILE *fp;

		if((fp = write_xml_file(*data)) == NULL){
			return -1;
		}
		if(reparse_xmlconf(&xd,fp) == 0){
			fprintf(stderr," Bad XML was accepted: %s.\n",*data);
			return -1;
		}
		printf(" Rejected: %s.\n",*data);
		xmlFreeDoc(xd);
		reset_logctx_ustrings();
	}
	return 0;
}

const declared_test XML_TESTS[] = {
	{	.name = "xmlaccept",
		.testfxn = test_xmlaccept_main,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "xmlreject",
		.testfxn = test_xmlreject_main,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTFAILED,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
