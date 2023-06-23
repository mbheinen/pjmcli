#include <curl/curl.h>
#include <expat_config.h>
#include <expat.h>

short debug_flag;

#define OFF	0
#define ON	1

typedef void (*QUERY_REQUEST_FUNC)(void *user_data, char *request_string, size_t size_of_request_string);

struct mg_query {
	char *name;                                    /* Name for the query */
	QUERY_REQUEST_FUNC get_request_string;         /* This is a callback function for constructing the Markets Gateway request string */
	XML_StartElementHandler start_element_handler; /* This is a callback function for expat. It runs at the start of an XML element */
	XML_EndElementHandler end_element_handler;     /* This is a callback function for expat. It runs at the end of an XML element */
	XML_CharacterDataHandler char_handler;         /* This is a callback function for expat. It runs for contents of an XML element */
	void *parse_handle;                            /* XML Parsing user data */
};

struct chunk 
{
	char *data;
	char *read_marker;
	size_t length;
	size_t size;
};

static void chunk_new(struct chunk *c)
{
	c->data = malloc(1);
	c->size = 0;
	c->length = 0;
}

static void chunk_init(struct chunk *c, const char *string)
{
	c->size = strlen(string);
	c->length = strlen(string);
	c->data = malloc(c->size + 1);
	memcpy(c->data, string, c->size + 1);
}

static void chunk_append(struct chunk *c, const char *string, int string_len)
{
	/* If string we are trying to store is longer than space available, increase size */
	if (string_len > c->size - c->length)
	{
		c->data = realloc(c->data, c->size + string_len + 1);
		c->size += string_len;
	}

	if(c->data == NULL)
		return;

	/* Append new data to chunk */
	memcpy(&(c->data[c->length]), string, string_len);

	/* Update length for added data and NULL terminate */
	c->length += string_len;
	c->data[c->length] = '\0';
}

static void chunk_clear(struct chunk *c)
{
	memset(c->data, 0, c->size);
	c->length = 0;
}

void chunk_free(struct chunk *c)
{
	free(c->data);

	c->data = NULL;
	c->read_marker = NULL;
	c->size = 0;
	c->length = 0;
}

/* Callback for libcurl to write something that was received over HTTP */
static size_t chunk_write_callback(void *contents, size_t size, size_t nmemb, void *user_data)
{
	size_t bytes = size * nmemb;
	struct chunk *c = (struct chunk *)user_data;
 
	c->data = realloc(c->data, c->size + bytes + 1);
	if(c->data == NULL)
		return 0;
 
	memcpy(&(c->data[c->size]), contents, bytes);
	c->size += bytes;
	c->data[c->size] = 0;

	if(debug_flag == ON) printf("%lu bytes recieved, %lu total\n", bytes, c->size);

	return bytes;
}

static void chunk_read_init(struct chunk *c)
{
	c->read_marker = c->data;
}

/* Callback for libcurl to read something that needs to be sent over HTTP */
static size_t chunk_read_callback(void *buffer, size_t size, size_t nmemb, void *user_data)
{
	struct chunk *c = (struct chunk *)user_data;
	size_t allowed_size = size * nmemb;

	if(c->length) 
	{
		/* Copy as much as possible from the chunk to the buffer */ 
		size_t copy_this_much = c->length;
		if(copy_this_much > allowed_size)
			copy_this_much = allowed_size;

		memcpy(buffer, c->read_marker, copy_this_much);

		c->read_marker += copy_this_much;
		c->length -= copy_this_much;

		/* we copied this many bytes */ 
		return copy_this_much; 
	}

	/* no more data left to deliver */ 
	return 0; 
}

/* Callback for libcurl to write something that was received over HTTP, since we are expecting to recieve XML
 * pass to expat parser */
static size_t xml_parse_write_callback(void *contents, size_t size, size_t nmemb, void *user_data)
{
	size_t bytes = size * nmemb;
	XML_Parser p = (XML_Parser)user_data;

	if(debug_flag == ON) printf("%u XML bytes recieved. They are:\n%.*s\n", bytes, bytes, contents);

	XML_Parse(p, contents, (int)bytes, 0);

	return bytes;
}

static int do_curl_common(CURL *curl, const char *url)
{
	CURLcode res;
	long http_code = 0;

	/* TODO: Add host verification if needed
	curl_easy_setopt(curl, CURLOPT_CAINFO, cert_path);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, verify_host ? 1L : 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, verify_host ? 2L : 0L);
	*/

	/* For now, don't verify host */
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	/* specify URL to get */ 
	curl_easy_setopt(curl, CURLOPT_URL, url);

	res = curl_easy_perform(curl);
	if(res != CURLE_OK)
	{
		if(debug_flag == ON) printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	}
	else
	{
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		if(debug_flag == ON) printf("HTTP Return Code: %d\n", http_code);
	}

	/* reset custom HTTP headers on curl handle */ 
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);

    return 0; /* return success */
}
 
static int do_curl_get(CURL *curl, const char *url, void *write_callback, void *write_user_data)
{
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

	/* send GET reseponse data to this function  */ 
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

	/* we pass our response chunk struct to the write callback function to write the response */ 
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_user_data);

	if(debug_flag == ON) printf("GET '%s'\n", url);

	return do_curl_common(curl, url);
}

static int do_curl_post(CURL *curl, const char *url, const char *body, void *write_callback, void *write_user_data)
{
	int ret;
	struct chunk c;

	chunk_init(&c, body);

	curl_easy_setopt(curl, CURLOPT_POST, 1L);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)c.size);
 
	/* set to use read callback for POST */ 
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, chunk_read_callback);
 
	/* set the user data that will be passed to the read function */
	chunk_read_init(&c);
	curl_easy_setopt(curl, CURLOPT_READDATA, &c);

	/* send reseponse data to this function  */ 
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

	/* set the user data that will be passed to the write function  */ 
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_user_data);

	if(debug_flag == ON) printf("POST '%s'\n", url);

	ret = do_curl_common(curl, url);
	
	chunk_free(&c);

	return ret;
}

struct market_results_result {
	float cleared_mw;
	time_t timestamp;
	char location[32];
	int index;
};

struct market_results_parser {
	struct chunk buffer;
	struct market_results_result result;
	time_t start_of_day_timestamp;
};
struct market_results_parser market_results_parse_handle;

static void market_results_request(struct tm *info, char *request_string, size_t size_of_request_string)
{
	char day_string[sizeof("YYYY-MM-DD")] = {0};

	strftime(day_string, sizeof(day_string), "%Y-%m-%d", info);

	snprintf(request_string, size_of_request_string, 
		"<QueryMarketResults type='Demand' day='%s'>"
		    "<All/>"
		"</QueryMarketResults>", day_string);
}

static void market_results_request_yesterday(void *user_data, char *request_string, size_t size_of_request_string)
{
	time_t current_time;
	struct tm *info;
	struct market_results_parser *parse_handle;

	parse_handle = user_data;

	time(&current_time);

	info = localtime(&current_time);
	info->tm_mday -= 1;
	mktime(info);
	info->tm_sec = 0;
	info->tm_min = 0;
	info->tm_hour = 0;
	parse_handle->start_of_day_timestamp = mktime(info);

	market_results_request(info, request_string, size_of_request_string);
}

static void market_results_request_today(void *user_data, char *request_string, size_t size_of_request_string)
{
	time_t current_time;
	struct tm *info;
	struct market_results_parser *parse_handle;

	parse_handle = user_data;

	time(&current_time);

	info = localtime(&current_time);

	info->tm_sec = 0;
	info->tm_min = 0;
	info->tm_hour = 0;
	parse_handle->start_of_day_timestamp = mktime(info);

	market_results_request(info, request_string, size_of_request_string);
}

static void market_results_request_tomorrow(void *user_data, char *request_string, size_t size_of_request_string)
{
	time_t current_time;
	struct tm *info;
	struct market_results_parser *parse_handle;

	parse_handle = user_data;

	time(&current_time);

	info = localtime(&current_time);
	info->tm_mday += 1;
	info->tm_sec = 0;
	info->tm_min = 0;
	info->tm_hour = 0;
	parse_handle->start_of_day_timestamp = mktime(info);

	market_results_request(info, request_string, size_of_request_string);
}

static void market_results_writer(struct market_results_result *result)
{
	char timestamp_string[sizeof("YYYY-MM-DD HH:mm:ss")] = {0};
	struct tm *info;

	info = localtime(&(result->timestamp));

	strftime(timestamp_string, sizeof(timestamp_string), "%Y-%m-%d %H:%M:%S", info);

	if(debug_flag == ON) printf("%s %d Cleared MW:%f\n", timestamp_string, result->timestamp, result->cleared_mw);
}

static void market_results_start_handler(void *user_data, const char *element, const char **attribute) 
{
	struct market_results_parser *parse_handle;
	int i;

	parse_handle = user_data;

	if (strcmp(element, "MarketResultsHourly") == 0)
	{
		for (i = 0; attribute[i]; i += 2) 
		{
			/* attribute[i] is the attribute name and attribute[i + 1] is the value*/
			if (strcmp("hour", attribute[i]) == 0)
			{
				parse_handle->result.timestamp = parse_handle->start_of_day_timestamp + atoi(attribute[i + 1]) * 3600;
			}
		}
	}
	else if (strcmp(element, "MarketResults") == 0)
	{
		memset(&(parse_handle->result), 0, sizeof(parse_handle->result));

		for (i = 0; attribute[i]; i += 2) 
		{
			/* attribute[i] is the attribute name and attribute[i + 1] is the value*/
			if (strcmp("location", attribute[i]) == 0)
				strcpy(parse_handle->result.location, attribute[i + 1]);
		}
	}
}

static void market_results_end_handler(void *user_data, const char *element) 
{
	struct market_results_parser *parse_handle;

	parse_handle = user_data;

	if (strcmp("MarketResultsHourly", element) == 0)
	{		
		market_results_writer(&(parse_handle->result));
	}
	else if (strcmp(element, "ClearedMW") == 0)
	{
		parse_handle->result.cleared_mw = (float)atof(parse_handle->buffer.data);
	}

	chunk_clear(&(parse_handle->buffer));
}

static void market_results_char_handler(void *user_data, const char *s, int len) 
{
	struct market_results_parser *parse_handle;

	parse_handle = user_data;

	/* Initialize chunk if empty */
	if(parse_handle->buffer.data == NULL)
		chunk_new(&(parse_handle->buffer));

	chunk_append(&(parse_handle->buffer), s, len);
}


#define MAX_MARKETS_GATEWAY_QUERY_REQUEST_SIZE 2048
#define MAX_MARKETS_GATEWAY_REQUEST_SIZE MAX_MARKETS_GATEWAY_QUERY_REQUEST_SIZE + 512

static int query_markets_gateway(CURL *curl, const char *session_token, struct mg_query *mg_query_h, int use_pjm_sandbox)
{
	int ret;
	struct curl_slist *headers = NULL;
	char session_cookie[256] = {0};
	char *request_string, *query_request_string;
	XML_Parser p;
	char *url;

	if(debug_flag == ON) printf("Peforming '%s' query\n", mg_query_h->name);

	/* Create expat XML parser handle */
	p = XML_ParserCreate(NULL);

	/* Setup expat XML parser */
	XML_SetElementHandler(p, mg_query_h->start_element_handler, mg_query_h->end_element_handler);
	XML_SetCharacterDataHandler(p, mg_query_h->char_handler);
	XML_SetUserData(p, mg_query_h->parse_handle);

	/* Create session cookie using the token */
	if(use_pjm_sandbox)
		snprintf(session_cookie, sizeof(session_cookie), "Cookie: pjmauthtrain=%s", session_token);
	else
		snprintf(session_cookie, sizeof(session_cookie), "Cookie: pjmauth=%s", session_token);

	if(debug_flag == ON) printf("Session cookie is '%s'\n", session_cookie); 
 
	/* Add HTTP headers */ 
	headers = curl_slist_append(headers, session_cookie);
	headers = curl_slist_append(headers, "Content-Type: text/xml");

	/* Assign headers to curl request */ 
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	/* Setup Markets Gateway request string */ 
	request_string = calloc(MAX_MARKETS_GATEWAY_REQUEST_SIZE, sizeof(*request_string));
	query_request_string = calloc(MAX_MARKETS_GATEWAY_QUERY_REQUEST_SIZE, sizeof(*query_request_string));

	mg_query_h->get_request_string(mg_query_h->parse_handle, query_request_string, MAX_MARKETS_GATEWAY_QUERY_REQUEST_SIZE);

	snprintf(request_string, MAX_MARKETS_GATEWAY_REQUEST_SIZE, 
		"<Envelope xmlns='http://schemas.xmlsoap.org/soap/envelope/'>"
		    "<Body>"
		        "<QueryRequest xmlns='http://emkt.pjm.com/emkt/xml'>"
		            "%s"
		        "</QueryRequest>"
		    "</Body>"
	    	"</Envelope>",
	       	query_request_string);

	if(debug_flag == ON) printf("Sending the following PJM Markets Gateway request:\n %s\n", request_string);

	/* Use PJM sandbox vs. production Markerts Gateway URL */ 
	url = use_pjm_sandbox ? "https://marketsgatewaytrain.pjm.com/marketsgateway/xml/query" : "https://marketsgateway.pjm.com/marketsgateway/xml/query";

	ret = do_curl_post(curl, url, request_string, xml_parse_write_callback, p);

	/* Send finalize to expat parser */
	XML_Parse(p, "", 0, 1);

	curl_slist_free_all(headers);
	XML_ParserFree(p);
	free(request_string);
	free(query_request_string);

	return ret;
}

static void get_openam_session_token(CURL *curl, char *user_name, char *password, char *token, size_t token_size, int use_pjm_sandbox)
{
	struct curl_slist *headers = NULL;
	struct chunk response;
	char username_header[64] = {0};
	char password_header[64] = {0};
	char *url;

	/* Create session cookie using the token */
	snprintf(username_header, sizeof(username_header), "X-OpenAM-Username: %s", user_name);
	snprintf(password_header, sizeof(password_header), "X-OpenAM-Password: %s", password);
 
	/* Add login HTTP headers */ 
	headers = curl_slist_append(headers, username_header);
	headers = curl_slist_append(headers, password_header);
 
	/* Set our custom set of headers */ 
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	/* Handle PJM sandbox vs. production URL */ 
	url = use_pjm_sandbox ? "https://ssotrain.pjm.com/access/authenticate/" : "https://sso.pjm.com/access/authenticate/";
	
	/* Initialize memory chunk to hold result */
	chunk_new(&response);

	/* Perform HTTP POST to PJM's Single Sign On framework (OpenAM) to get session token */ 
	do_curl_post(curl, url, "", chunk_write_callback, &response);

	if(debug_flag == ON) printf("OpenAM query response:\n %s\n", response.data);

	/* TODO: sscanf is brittle, replace */
	if(sscanf(response.data, "{\"tokenId\":\"%98s\",\"successUrl\":\"/access/console\",\"realm\":\"/\"}", token) != 1)
		if(debug_flag == ON) printf("Failed to parse OpenAM session token\n");

	chunk_free(&response);
}


struct mg_query mg_queries[] = {
	{
		"Market Results Yesterday",
		market_results_request_yesterday,
		market_results_start_handler,
		market_results_end_handler,
		market_results_char_handler,
		&(market_results_parse_handle)
	},
	{
		"Market Results Today",
		market_results_request_today,
		market_results_start_handler,
		market_results_end_handler,
		market_results_char_handler,
		&(market_results_parse_handle)
	},
	{
		"Market Results Tomorrow",
		market_results_request_tomorrow,
		market_results_start_handler,
		market_results_end_handler,
		market_results_char_handler,
		&(market_results_parse_handle)
	}
};

enum mg_query_type {
	MARKET_RESULTS_YESTERDAY,
	MARKET_RESULTS_TODAY,
	MARKET_RESULTS_TOMORROW,
	MARKET_QUERY_LEN
};

/* Command line tool to query PJM Markets Gateway web service using libcurl (https://curl.haxx.se/libcurl/)
 * for HTTP client, expat to parse XML (https://libexpat.github.io/), and writes to command line. Details
 * of the PJM Markets Gateway web service can be found here:
 *
 * https://www.pjm.com/-/media/etools/emkt/external-interface-specification-guide-revision.ashx?la=en 
 */
int main(int argc, char *argv[])
{
	CURL *curl;
	char session_token[128] = {0};
	char *username, *password;
	int use_pjm_sandbox;

	use_pjm_sandbox = 0;
	username = getenv("PJM_USERNAME");
	password = getenv("PJM_PASSWORD");
	debug_flag = ON;
  
	if(debug_flag == ON) printf("username: %s\n", username != NULL ? username : "Not set! (Set PJM_USERNAME environment variable)");
	if(debug_flag == ON) printf("password: %s\n", password != NULL ? "******" : "Not set! (Set PJM_PASSWORD environment variable)");

	/* Initialize libcurl session */ 
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	/* Get PJM OpenAM session token*/
	get_openam_session_token(curl, username, password, session_token, sizeof(session_token), use_pjm_sandbox);
	if(debug_flag == ON) printf("OpenAM session token: %s\n", session_token);

	/* Query Markets Gateway web service for yesterday */
	query_markets_gateway(curl, session_token, &(mg_queries[MARKET_RESULTS_YESTERDAY]), use_pjm_sandbox);

	/* Query Markets Gateway web service for today */
	query_markets_gateway(curl, session_token, &(mg_queries[MARKET_RESULTS_TODAY]), use_pjm_sandbox);

	/* Query Markets Gateway web service for tomorrow */
	query_markets_gateway(curl, session_token, &(mg_queries[MARKET_RESULTS_TOMORROW]), use_pjm_sandbox);

	exit(1);
}
