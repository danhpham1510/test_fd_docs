#include "network_interface.h"
#include "esp_err.h"
#include <sys/param.h>
#include "esp_netif.h"
#include <esp_http_server.h>
#include "cJSON.h"
#include <string.h>
#include "handle_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_tls_crypto.h"
#include "handle_spiffs.h"
#include "lwip/sys.h"
#include "lwip/dns.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "peripherals_interface.h"

#define ESP_WIFI_SSID "Danh-ESP32"
#define ESP_WIFI_PASS "password"
#define NUM_WIFI_CHANNEL 1
#define MAX_STA_CON 1
#define AUTH_USR "devmaster"
#define AUTH_PWD "12345678"
#define portTICK_PERIOD_MS ((TickType_t)(1000 / configTICK_RATE_HZ))

static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *netif_ap;
static esp_netif_t *netif_sta;
static httpd_handle_t server = NULL;
static const char* TAG = "webserver";

typedef struct
{
	char* username;
	char* password;
} basic_auth_info_t;

#define HTTPD_401 "401 UNAUTHORIZED" 

/**
 * @brief Creates basic authentication key from username and password
 * 
 * @param username Authentication username 
 * @param password Authentication password
 * @return char* The Basic auth key
 */
static char* http_auth_basic(const char* username, const char* password)
{
	int out;
	char* user_info = NULL;
	char* digest = NULL;
	size_t n = 0;
	asprintf(&user_info, "%s:%s", username, password);
	if (!user_info) {
		ESP_LOGE(TAG, "No enough memory for user information");
		return NULL;
	}
	esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char* )user_info, strlen(user_info));

	/* 6: The length of the "Basic " string
	 * n: Number of bytes for a base64 encode format
	 * 1: Number of bytes for a reserved which be used to fill zero
	 */
	digest = calloc(1, 6 + n + 1);
	if (digest) {
		strcpy(digest, "Basic ");
		esp_crypto_base64_encode((unsigned char* )digest + 6, n, (size_t *)&out, (const unsigned char* )user_info, strlen(user_info));
	}
	free(user_info);
	return digest;
}

/**
 * @brief Verify the authentication key
 * 
 * @param req The HTTP request
 * @return true Authenticated
 * @return false Authentication fail
 */
static bool check_auth(httpd_req_t *req)
{
	char* buf = NULL;
	size_t buf_len = 0;
	basic_auth_info_t *basic_auth_info = req->user_ctx;

	buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
	if (buf_len > 1) {
		buf = calloc(1, buf_len);
		if (!buf) {
			ESP_LOGE(TAG, "Not enough memory for basic authorization");
			free(buf);
			return false;
		}
		if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
			ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
		} else {
			ESP_LOGE(TAG, "No auth value received");
		}

		char* auth_credentials = http_auth_basic(basic_auth_info->username, basic_auth_info->password);
		if (!auth_credentials) {
			ESP_LOGE(TAG, "Not enough memory for basic authorization credentials");
			free(buf);
			return false;
		}
		if (strncmp(auth_credentials, buf, buf_len)) {
			ESP_LOGE(TAG, "Not authenticated");
			httpd_resp_set_status(req, HTTPD_401);
			httpd_resp_set_type(req, "application/json");
			httpd_resp_set_hdr(req, "Connection", "keep-alive");
			char response[] = "Authentication failed.";
			httpd_resp_send(req, response, strlen(response));
			free(buf);
			return false;
		} else {
			ESP_LOGI(TAG, "Authenticated!");
			return true;
		}

	} else {
		ESP_LOGE(TAG, "No auth header received");
		char response[] = "Authentication failed.";
		httpd_resp_set_status(req, HTTPD_401);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		httpd_resp_send(req, response, strlen(response));
		free(buf);
		return false;
	}
}

// =========================== HTTP WEBSERVER CODE ============================================================================================================================
/**
 * @brief Handler for /mqtt/info endpoint
 * 
 * @param req The requests
 * @return esp_err_t ESP error code
 */
static esp_err_t mqtt_info_get_handler(httpd_req_t *req)
{
	esp_err_t error;
	if (check_auth(req)) {
		char* basic_auth_resp = read_mqtt_cfg();
		httpd_resp_set_status(req, HTTPD_200);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		if (!basic_auth_resp) {
			ESP_LOGE(TAG, "Not enough memory for basic authorization response");
			return ESP_ERR_NO_MEM;
		}
		error = httpd_resp_send(req, basic_auth_resp, strlen(basic_auth_resp));
	} else {
		httpd_resp_set_status(req, HTTPD_401);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		char response[] = "Authentication failed.";
		error = httpd_resp_send(req, response, strlen(response));
	}

	if (error != ESP_OK) {
		ESP_LOGI(TAG, "Error %d while sending Response", error);
	} else
		ESP_LOGI(TAG, "Response sent Successfully");
	return error;
}

static httpd_uri_t mqtt_info = {
	.uri = "/mqtt/info",
	.method = HTTP_GET,
	.handler = mqtt_info_get_handler,
};

/**
 * @brief Handler for /wifi/info endpoint
 * 
 * @param req The request
 * @return esp_err_t ESP error code
 */
static esp_err_t wifi_info_get_handler(httpd_req_t *req)
{
	esp_err_t error;
	if (check_auth(req)) {
		char* basic_auth_resp = read_wifi_cfg();
		httpd_resp_set_status(req, HTTPD_200);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		// asprintf(&basic_auth_resp, "{\"authenticated\": true,\"user\": \"%s\"}", basic_auth_info->username);
		if (!basic_auth_resp){
			ESP_LOGE(TAG, "Not enough memory for basic authorization response");
			return ESP_ERR_NO_MEM;
		}
		error = httpd_resp_send(req, basic_auth_resp, strlen(basic_auth_resp));
	} else {
		httpd_resp_set_status(req, HTTPD_401);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		char response[] = "Authentication failed.";
		error = httpd_resp_send(req, response, strlen(response));
	}
	if (error != ESP_OK) {
		ESP_LOGI(TAG, "Error %d while sending Response", error);
	} else
		ESP_LOGI(TAG, "Response sent Successfully");
	return error;
}

static httpd_uri_t wifi_info = {
	.uri = "/wifi/info",
    	.method = HTTP_GET,
    	.handler = wifi_info_get_handler,
};

/**
 * @brief Handler for /conn endpoint
 * 
 * @param req The request
 * @return esp_err_t ESP error code
 */
esp_err_t conn_handler(httpd_req_t *req)
{
	esp_err_t error;
	if (check_auth(req)) {
		char* basic_auth_resp = read_uuid();
		httpd_resp_set_status(req, HTTPD_200);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		// asprintf(&basic_auth_resp, "{\"authenticated\": true,\"user\": \"%s\"}", basic_auth_info->username);
		if (!basic_auth_resp) {
			ESP_LOGE(TAG, "Not enough memory for basic authorization response");
			return ESP_ERR_NO_MEM;
		}
		error = httpd_resp_send(req, basic_auth_resp, strlen(basic_auth_resp));
	} else {
		httpd_resp_set_status(req, HTTPD_401);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		char response[] = "Authentication failed.";
		error = httpd_resp_send(req, response, strlen(response));
	} if (error != ESP_OK) {
		ESP_LOGI(TAG, "Error %d while sending Response", error);
	} else
		ESP_LOGI(TAG, "Response sent Successfully");
	return error;
}

static httpd_uri_t conn = {
    	.uri = "/conn",
    	.method = HTTP_GET,
    	.handler = conn_handler,
};

/**
 * @brief Handler for /index endpoint
 * 
 * @param req The request
 * @return esp_err_t ESP error code
 *
 */
static esp_err_t index_handler(httpd_req_t *req)
{
	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	// CSS
	httpd_resp_sendstr_chunk(req, "<style>");
	httpd_resp_sendstr_chunk(req, "body {");
	httpd_resp_sendstr_chunk(req, "  font-family: Arial;");
	httpd_resp_sendstr_chunk(req, "}");

	httpd_resp_sendstr_chunk(req, "h1 {text-align: center;}");
	httpd_resp_sendstr_chunk(req, "h2 {text-align: center;}");
	httpd_resp_sendstr_chunk(req, "h3 {text-align: center;}");

	httpd_resp_sendstr_chunk(req, "input[type=text],input[type=password] {");
	httpd_resp_sendstr_chunk(req, "  width: 100%;");
	httpd_resp_sendstr_chunk(req, "  padding: 12px 20px;");
	httpd_resp_sendstr_chunk(req, "  margin: 8px 0;");
	httpd_resp_sendstr_chunk(req, "  display: block;");
	httpd_resp_sendstr_chunk(req, "  border: 1px solid #ccc;");
	httpd_resp_sendstr_chunk(req, "  box-sizing: border-box;");
	httpd_resp_sendstr_chunk(req, "}");

	httpd_resp_sendstr_chunk(req, "input[type=submit], {");
	httpd_resp_sendstr_chunk(req, "  width: 100%;");
	httpd_resp_sendstr_chunk(req, "  background-color: #04AA6D;");
	httpd_resp_sendstr_chunk(req, "  color: white;");
	httpd_resp_sendstr_chunk(req, "  padding: 14px 20px;");
	httpd_resp_sendstr_chunk(req, "  margin: 8px 0;");
	httpd_resp_sendstr_chunk(req, "  border: none;");
	httpd_resp_sendstr_chunk(req, "  border-radius: 4px;");
	httpd_resp_sendstr_chunk(req, "  cursor: pointer;");
	httpd_resp_sendstr_chunk(req, "}");

	httpd_resp_sendstr_chunk(req, "input[type=submit]:hover {");
	httpd_resp_sendstr_chunk(req, "  background-color: #45a049;");
	httpd_resp_sendstr_chunk(req, "}");

	httpd_resp_sendstr_chunk(req, "div.container {");
	httpd_resp_sendstr_chunk(req, "  border-radius: 0px;");
	httpd_resp_sendstr_chunk(req, "  background-color: #f2f2f2;");
	httpd_resp_sendstr_chunk(req, "  text-align: left;");
	httpd_resp_sendstr_chunk(req, "  margin: auto;");
	httpd_resp_sendstr_chunk(req, "  width: 60%;");
	httpd_resp_sendstr_chunk(req, "  border: 3px solid;");
	httpd_resp_sendstr_chunk(req, "  padding: 10px;");
	httpd_resp_sendstr_chunk(req, "}");
	httpd_resp_sendstr_chunk(req, "</style>");
	// Body
	// Wifi Form
	httpd_resp_sendstr_chunk(req, "<body>");
	httpd_resp_sendstr_chunk(req, "<h1>Aura Fall Detection provisioning</h1>");
	httpd_resp_sendstr_chunk(req, "<br></br>");
	httpd_resp_sendstr_chunk(req, "<h3>Wi-Fi Credentials</h3>");
	httpd_resp_sendstr_chunk(req, "<div class=\"container\">");
	//  Get current wifi creds in spiffs
	char* wifi_json = read_wifi_cfg();
	char* my_ssid = parse_wifi_ssid(wifi_json);
	char* my_pwd = parse_wifi_pwd(wifi_json);
	char ssid_input[200];
	char pwd_input[200];
	// Show current creds as placeholder
	snprintf(ssid_input, 200, "<input type=\"text\" id=\"ssid\" name=\"ssid\" placeholder='%s' maxlength=\"50\" required>", my_ssid);
	snprintf(pwd_input, 200, "<input type=\"password\" id=\"password\" name=\"password\" placeholder='%s' minlength=\"8\" maxlength=\"24\" required>", my_pwd);

	httpd_resp_sendstr_chunk(req, " <form name = \"Wifi Credentials\" id = \"wifiForm\" action = \"\" onSubmit=\"if(!alert('WiFi Credentials submitted')){window.location.reload();}\">");
	httpd_resp_sendstr_chunk(req, "<label for=\"ssid\">SSID</label>");
	httpd_resp_sendstr_chunk(req, ssid_input);
	httpd_resp_sendstr_chunk(req, "<label for=\"password\">Wi-Fi password</label>");
	httpd_resp_sendstr_chunk(req, pwd_input);
	httpd_resp_sendstr_chunk(req, "<input type=\"submit\" value=\"Submit\">");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "</div>");
	// MQTT Form
	// Get current MQTT creds in spiffs

	char* mqtt_json = read_mqtt_cfg();
	char* mqtt_hostname = parse_mqtt_hostname(mqtt_json);
	char* mqtt_port = parse_mqtt_port(mqtt_json);
	char* mqtt_username = parse_mqtt_username(mqtt_json);
	char* mqtt_password = parse_mqtt_password(mqtt_json);
	static char hostname_input[200];
	static char port_input[200];
	static char username_input[200];
	static char mq_pw_input[200];
	// Show current creds as placeholder
	snprintf(hostname_input, 200, "<input type=\"text\" id=\"mqtt_hostname\" name=\"mqtt_hostname\" placeholder='%s' maxlength=\"50\" required>", mqtt_hostname);
	snprintf(port_input, 200, "<input type=\"text\" id=\"mqtt_port\" name=\"mqtt_port\" placeholder='%s' maxlength=\"5\" required>", mqtt_port);
	snprintf(username_input, 200, "<input type=\"text\" id=\"mqtt_username\" name=\"mqtt_username\" placeholder='%s' maxlength=\"50\" required>", mqtt_username);
	snprintf(mq_pw_input, 200, "<input type=\"password\" id=\"mqtt_password\" name=\"mqtt_password\" placeholder='%s' minlength=\"8\" maxlength=\"24\" required>", mqtt_password);
	httpd_resp_sendstr_chunk(req, "<br></br>");
	httpd_resp_sendstr_chunk(req, "<h3>MQTT Credentials</h3>");
	httpd_resp_sendstr_chunk(req, "<div class=\"container\">");
	httpd_resp_sendstr_chunk(req, "<form name = \"MQTT Credentials\" id = \"mqttForm\" action = \"\"    onSubmit=\"if(!alert('MQTT Credentials submitted')){window.location.reload();}\">");
	httpd_resp_sendstr_chunk(req, "<label for=\"mqtt_hostname\">Hostname</label>");
	httpd_resp_sendstr_chunk(req, hostname_input);
	httpd_resp_sendstr_chunk(req, "<label for=\"mqtt_port\">Port</label>");
	httpd_resp_sendstr_chunk(req, port_input);
	httpd_resp_sendstr_chunk(req, "<label for=\"mqtt_username\">MQTT Username</label>");
	httpd_resp_sendstr_chunk(req, username_input);
	httpd_resp_sendstr_chunk(req, "<label for=\"mqtt_password\">MQTT Password</label>");
	httpd_resp_sendstr_chunk(req, mq_pw_input);
	httpd_resp_sendstr_chunk(req, "<input type=\"submit\" value=\"Submit\">");
	httpd_resp_sendstr_chunk(req, "  </form>");
	httpd_resp_sendstr_chunk(req, "</div>");

	static char wifi_post[600];
	static char mqtt_post[600];
	char* basic_auth = "'Basic ZGV2bWFzdGVyOjEyMzQ1Njc4'";
	snprintf(wifi_post, 600, "document.getElementById(\"wifiForm\").addEventListener(\"submit\", (e) => {e.preventDefault(); const formData = new FormData(e.target); const data = Array.from(formData.entries()).reduce((memo, pair) => ({...memo, [pair[0]]: pair[1],  }), {}); var xhr = new XMLHttpRequest(); xhr.open(\"POST\", \"http://192.168.4.1/wifi/creds\", true);xhr.setRequestHeader('Authorization', %s); xhr.setRequestHeader('Content-Type', 'application/json'); xhr.send(JSON.stringify(data)); document.getElementById(\"output\").innerHTML = JSON.stringify(data);});", basic_auth);
	snprintf(mqtt_post, 600, "document.getElementById(\"mqttForm\").addEventListener(\"submit\", (e) => {e.preventDefault(); const formData = new FormData(e.target); const data = Array.from(formData.entries()).reduce((memo, pair) => ({...memo, [pair[0]]: pair[1],[pair[2]]: pair[3]  }), {}); var xhr = new XMLHttpRequest(); xhr.open(\"POST\", \"http://192.168.4.1/mqtt/creds\", true); xhr.setRequestHeader('Authorization', %s);xhr.setRequestHeader('Content-Type', 'application/json'); xhr.send(JSON.stringify(data)); document.getElementById(\"output\").innerHTML = JSON.stringify(data);});", basic_auth);
	httpd_resp_sendstr_chunk(req, "<script>");
	// httpd_resp_sendstr_chunk(req, "document.getElementById(\"wifiForm\").addEventListener(\"submit\", (e) => {e.preventDefault(); const formData = new FormData(e.target); const data = Array.from(formData.entries()).reduce((memo, pair) => ({...memo, [pair[0]]: pair[1],  }), {}); var xhr = new XMLHttpRequest(); xhr.open(\"POST\", \"http://192.168.4.1/wifi/creds\", true);xhr.setRequestHeader('Authorization', 'Basic ZGV2bWFzdGVyOjEyMzQ1Njc4'); xhr.setRequestHeader('Content-Type', 'application/json'); xhr.send(JSON.stringify(data)); document.getElementById(\"output\").innerHTML = JSON.stringify(data);});");
	httpd_resp_sendstr_chunk(req, wifi_post);
	httpd_resp_sendstr_chunk(req, mqtt_post);
	// httpd_resp_sendstr_chunk(req, "document.getElementById(\"mqttForm\").addEventListener(\"submit\", (e) => {e.preventDefault(); const formData = new FormData(e.target); const data = Array.from(formData.entries()).reduce((memo, pair) => ({...memo, [pair[0]]: pair[1],[pair[2]]: pair[3]  }), {}); var xhr = new XMLHttpRequest(); xhr.open(\"POST\", \"http://192.168.4.1/mqtt/creds\", true); xhr.setRequestHeader('Authorization', 'Basic ZGV2bWFzdGVyOjEyMzQ1Njc4');xhr.setRequestHeader('Content-Type', 'application/json'); xhr.send(JSON.stringify(data)); document.getElementById(\"output\").innerHTML = JSON.stringify(data);});");
	httpd_resp_sendstr_chunk(req, "</script>");
	httpd_resp_sendstr_chunk(req, "</body>");
	httpd_resp_sendstr_chunk(req, "</html>");
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

static const httpd_uri_t wf_ui = {
    	.uri = "/index",
    	.method = HTTP_GET,
    	.handler = index_handler,
    	.user_ctx = NULL};

/**
 * @brief Handler for /mqtt/creds endpoint
 * 
 * @param req request
 * @return esp_err_t ESP error code
 */
static esp_err_t mqtt_creds_handler(httpd_req_t *req)
{
	esp_err_t error;
	char content[400];
	/* Truncate if content length larger than the buffer */
	size_t recv_size = MIN(req->content_len, sizeof(content));
	int ret = httpd_req_recv(req, content, recv_size);
	if (ret <= 0) {
		if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
			/* In case of timeout one can choose to retry calling
			 * httpd_req_recv(), but to keep it simple, here we
			 * respond with an HTTP 408 (Request Timeout) error */
			httpd_resp_send_408(req);
		}
		/* In case of error, returning ESP_FAIL will
		 * ensure that the underlying socket is closed */
		return ESP_FAIL;
	}
	// Checking Authorization
	if (check_auth(req)) {
		httpd_resp_set_status(req, HTTPD_200);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		// Handle and print post message
		cJSON *mqtt_hostname = NULL;
		cJSON *mqtt_port = NULL;
		cJSON *mqtt_username = NULL;
		cJSON *mqtt_password = NULL;
		cJSON *recv_json = cJSON_Parse(content);
		if (recv_json == NULL) {
			const char resp[] = "Failed to parse json";
			error = httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
		} else {
			if (cJSON_HasObjectItem(recv_json, "mqtt_hostname") && cJSON_HasObjectItem(recv_json, "mqtt_port") && cJSON_HasObjectItem(recv_json, "mqtt_username") && cJSON_HasObjectItem(recv_json, "mqtt_password")) {
				mqtt_hostname = cJSON_GetObjectItemCaseSensitive(recv_json, "mqtt_hostname");
				mqtt_port = cJSON_GetObjectItemCaseSensitive(recv_json, "mqtt_port");
				mqtt_username = cJSON_GetObjectItemCaseSensitive(recv_json, "mqtt_username");
				mqtt_password = cJSON_GetObjectItemCaseSensitive(recv_json, "mqtt_password");
				if ((mqtt_hostname->valuestring != NULL) && (mqtt_port->valuestring != NULL) && (mqtt_username->valuestring != NULL) && (mqtt_password->valuestring != NULL)) {
					write_mqtt_cfg(mqtt_hostname->valuestring, mqtt_port->valuestring, mqtt_username->valuestring, mqtt_password->valuestring);
					const char resp[] = "Complete buffer received";
					error = httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
				} else {
					const char resp[] = "Incorrect message format, parameters has to be in string format";
					error = httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
				}
			} else {
				const char resp[] = "Incorrect message format";
				error = httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
			}
		}
		cJSON_Delete(recv_json);
	} else {
		httpd_resp_set_status(req, HTTPD_401);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		char response[] = "Authentication failed.";
		error = httpd_resp_send(req, response, strlen(response));
	}
	if (error != ESP_OK) {
		ESP_LOGI(TAG, "Error %d while sending Response", error);
	} else
		ESP_LOGI(TAG, "Response sent Successfully");
	return error;
}

static httpd_uri_t mqtt_creds = {
    	.uri = "/mqtt/creds",
    	.method = HTTP_POST,
    	.handler = mqtt_creds_handler
};

/**
 * @brief Handler for /wifi/creds endpoint
 * 
 * @param req The request
 * @return esp_err_t ESP error code
 */
static esp_err_t wifi_creds_handler(httpd_req_t *req)
{
	esp_err_t error;
	char content[400];
	/* Truncate if content length larger than the buffer */
	size_t recv_size = MIN(req->content_len, sizeof(content));
	int ret = httpd_req_recv(req, content, recv_size);
	
	if (ret <= 0) {
		if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
			/* In case of timeout one can choose to retry calling
			 * httpd_req_recv(), but to keep it simple, here we
			 * respond with an HTTP 408 (Request Timeout) error */
			httpd_resp_send_408(req);
		}
		/* In case of error, returning ESP_FAIL will
		 * ensure that the underlying socket is closed */
		return ESP_FAIL;
	}
	// Checking Authorization
	if (check_auth(req)) {
		httpd_resp_set_status(req, HTTPD_200);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		// Handle and print post message
		cJSON *ssid = NULL;
		cJSON *password = NULL;
		cJSON *recv_json = cJSON_Parse(content);
		if (recv_json == NULL) {
			const char resp[] = "Failed to parse json";
			error = httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
		} else {
			if (cJSON_HasObjectItem(recv_json, "ssid") && cJSON_HasObjectItem(recv_json, "password")) {
				ssid = cJSON_GetObjectItemCaseSensitive(recv_json, "ssid");
				password = cJSON_GetObjectItemCaseSensitive(recv_json, "password");
				if ((ssid->valuestring != NULL) && (password->valuestring != NULL)) {
					write_wifi_cfg(ssid->valuestring, password->valuestring);
					const char resp[] = "Complete buffer received";
					error = httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
				} else {
					const char resp[] = "Incorrect message format, parameters has to be in string format";
					error = httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
				}
			} else {
				const char resp[] = "Incorrect message format";
				error = httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
			}
		}
		cJSON_Delete(recv_json);
	} else {
		httpd_resp_set_status(req, HTTPD_401);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		char response[] = "Authentication failed.";
		error = httpd_resp_send(req, response, strlen(response));
	}
	if (error != ESP_OK) {
		ESP_LOGI(TAG, "Error %d while sending Response", error);
	} else
		ESP_LOGI(TAG, "Response sent Successfully");
	return error;
}

static httpd_uri_t wifi_creds = {
    	.uri = "/wifi/creds",
    	.method = HTTP_POST,
    	.handler = wifi_creds_handler
};

/**
 * @brief Handelr for 404 error
 * 
 * @param req The request
 * @param err The HTTP error code
 * @return esp_err_t The error code
 */
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
	/* For any other URI send 404 and close socket */
	httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
	return ESP_FAIL;
}

/**
 * @brief Register handlers and endpoints, along side the Auth info
 * 
 * @param server The HTTP server
 */
static void httpd_register_basic_auth(httpd_handle_t server)
{
	basic_auth_info_t *basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
	if (basic_auth_info) {
		basic_auth_info->username = AUTH_USR;
		basic_auth_info->password = AUTH_PWD;

		mqtt_info.user_ctx = basic_auth_info;
		mqtt_creds.user_ctx = basic_auth_info;
		wifi_info.user_ctx = basic_auth_info;
		wifi_creds.user_ctx = basic_auth_info;
		conn.user_ctx = basic_auth_info;

		httpd_register_uri_handler(server, &mqtt_info);
		httpd_register_uri_handler(server, &mqtt_creds);
		httpd_register_uri_handler(server, &wifi_info);
		httpd_register_uri_handler(server, &wifi_creds);
		httpd_register_uri_handler(server, &conn);
		httpd_register_uri_handler(server, &wf_ui);
	}
}

/**
 * @brief Start webserver
 * 
 * @return httpd_handle_t The server
 */
static httpd_handle_t start_webserver(void)
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.lru_purge_enable = true;
	// Start the httpd server
	ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) == ESP_OK) {
		// Set URI handlers
		ESP_LOGI(TAG, "Registering URI handlers");
		httpd_register_basic_auth(server);
		return server;
	}
	ESP_LOGI(TAG, "Error starting server!");
	return NULL;
}


// =========================== END OF WEBSERVER CODE =======================================================================================================================
/**
 * @brief Handler for all wifi event
 * 
 * @param arg 
 * @param event_base Event base 
 * @param event_id Event ID
 * @param event_data Event Data
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (WIFI_EVENT == event_base) {
		switch (event_id) {
		case WIFI_EVENT_AP_STACONNECTED:; // when a new device is connected to the AP
			wifi_event_ap_staconnected_t *connected_event = (wifi_event_ap_staconnected_t *)event_data;
			ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(connected_event->mac), connected_event->aid);
			break;

		case WIFI_EVENT_AP_STADISCONNECTED:; // when a device disconnect with the AP
			wifi_event_ap_stadisconnected_t *disconnect_event = (wifi_event_ap_stadisconnected_t *)event_data;
			ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(disconnect_event->mac), disconnect_event->aid);
			break;

		case WIFI_EVENT_STA_START: // when wifi_init_sta is called
			esp_wifi_connect();
			break;

		case WIFI_EVENT_STA_DISCONNECTED: // when wifi router lost connection
			ESP_LOGE(TAG, "Retrying to connect with the AP");
			esp_wifi_connect();
			break;

		default:
			ESP_LOGI(TAG, "Unknown event id: %d", event_id);
			break;
		}
	} else if (IP_EVENT == event_base) {
		switch (event_id) {
		case IP_EVENT_STA_GOT_IP:; // When connected to AP
			ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
			ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		}
	}
}

// Init and change device into softap mode
void wifi_init_softap(void)
{
	esp_netif_ip_info_t ipInfo;
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	netif_ap = esp_netif_create_default_wifi_ap();

	IP4_ADDR(&ipInfo.ip, 192, 168, 4, 1);
	IP4_ADDR(&ipInfo.gw, 192, 168, 4, 1);
	IP4_ADDR(&ipInfo.netmask, 255, 255, 255, 0);

	esp_netif_dhcps_stop(netif_ap);
	esp_netif_set_ip_info(netif_ap, &ipInfo);
	esp_netif_dhcps_start(netif_ap);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
							    ESP_EVENT_ANY_ID,
							    &wifi_event_handler,
							    NULL,
							    NULL));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
							    ESP_EVENT_ANY_ID,
							    &wifi_event_handler,
							    NULL,
							    NULL));

	wifi_config_t ap_config = {
	    .ap = {
		.ssid = ESP_WIFI_SSID,
		.ssid_len = strlen(ESP_WIFI_SSID),
		.channel = NUM_WIFI_CHANNEL,
		.password = ESP_WIFI_PASS,
		.max_connection = MAX_STA_CON,
		.authmode = WIFI_AUTH_WPA_WPA2_PSK},
	};
	if (strlen(ESP_WIFI_PASS) == 0) {
		ap_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
		 ESP_WIFI_SSID, ESP_WIFI_PASS, NUM_WIFI_CHANNEL);

	// start webserver after turning on ap
	if (server == NULL) {
		ESP_LOGI(TAG, "Starting webserver");
		server = start_webserver();
	}
}

// Init and change device into sta mode, connect to the designated ssid
esp_err_t wifi_init_sta()
{
	esp_err_t ret_value = ESP_OK;
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	netif_sta = esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

	char* wifi_json = read_wifi_cfg();
	char* my_ssid = parse_wifi_ssid(wifi_json);
	char* my_pwd = parse_wifi_pwd(wifi_json);


	wifi_config_t wifi_config = {
	    .sta = {
		.ssid = {0},
		.password = {0}},
	};
	memset(wifi_config.sta.ssid, 0, 32);
	memset(wifi_config.sta.password, 0, 64);
	memcpy(wifi_config.sta.ssid, my_ssid, strlen(my_ssid));
	memcpy(wifi_config.sta.password, my_pwd, strlen(my_pwd));
	
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ret_value = esp_wifi_start();
	return ret_value;
}

