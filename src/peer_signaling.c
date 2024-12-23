#ifndef DISABLE_PEER_SIGNALING
#include <assert.h>
#include <cJSON.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "base64.h"
#include "config.h"
#include "peer_signaling.h"
#include "ports.h"
#include "utils.h"
#include <libwebsockets.h>
#include <json-glib/json-glib.h>

#define KEEP_ALIVE_TIMEOUT_SECONDS 60
#define CONNACK_RECV_TIMEOUT_MS 1000

#define TOPIC_SIZE 128
#define HOST_LEN 64
#define CRED_LEN 128
#define PEER_ID_SIZE 100

#define RPC_VERSION "2.0"

#define RPC_METHOD_STATE "state"
#define RPC_METHOD_OFFER "offer"
#define RPC_METHOD_ANSWER "answer"
#define RPC_METHOD_CLOSE "close"

#define RPC_ERROR_PARSE_ERROR "{\"code\":-32700,\"message\":\"Parse error\"}"
#define RPC_ERROR_INVALID_REQUEST "{\"code\":-32600,\"message\":\"Invalid Request\"}"
#define RPC_ERROR_METHOD_NOT_FOUND "{\"code\":-32601,\"message\":\"Method not found\"}"
#define RPC_ERROR_INVALID_PARAMS "{\"code\":-32602,\"message\":\"Invalid params\"}"
#define RPC_ERROR_INTERNAL_ERROR "{\"code\":-32603,\"message\":\"Internal error\"}"

static char* rand_string(char* str, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK...";
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int)(sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

static char * get_string_from_json_object(JsonObject *object) {
    JsonNode *root;
    JsonGenerator *generator;
    gchar *text;

    root = json_node_init_object(json_node_alloc(), object);
    generator = json_generator_new();
    json_generator_set_root(generator, root);
    text = json_generator_to_data(generator, NULL);

    g_object_unref(generator);
    json_node_free(root);

    return text;
}

typedef struct PeerSignaling {

    char subtopic[TOPIC_SIZE];
    char pubtopic[TOPIC_SIZE];

    uint16_t packet_id;
    char *id;

    int mqtt_port;
    int http_port;
    char mqtt_host[HOST_LEN];
    char http_host[HOST_LEN];
    char http_path[HOST_LEN];
    char ws_host[HOST_LEN];
    int  ws_port;
    char username[CRED_LEN];
    char password[CRED_LEN];
    char client_id[CRED_LEN];
    PeerConnection* pc;

} PeerSignaling;

enum MsgType {
    JANUS_MSS_UNKNOWN = 0,
    JANUS_MSS_ICE_CANDIDATE = 1,
    JANUS_MSS_ICE_CANDIDATE_GATHERED = 2,
    JANUS_MSS_SDP_OFFER = 3,
    JANUS_MSS_SDP_ANSWER = 4,
    JANUS_MSS_REGISTER = 5,
    JANUS_MSS_REGISTER_WITH_SERVER = 6,
    JANUS_MSS_ROOM_REQUEST = 7,
    JANUS_MSS_KEEP_ALIVE = 8,
    JANUS_MSS_ROOM_CALL = 9,
    ///////////////////////////////////////////////////////////
    JANUS_MSS_ROOM_PARTICIPANTS = 10,
    //////////////////////////////////////////////////////////
};

static PeerSignaling g_ps;

//Janus information
long long feed_id = 0;
long long session_id = 0;
long long handle_id = 0;
char transaction[12];
static struct lws* web_socket = NULL;
static const char* room = "1234";
static const char* token = "";
static const char* receiver_id = "2002";
static const char* peer_id = "1001";
static int mainloop = 1;
enum AppState {
    APP_STATE_UNKNOWN = 0,
    APP_STATE_ERROR = 1, /* generic error */
    SERVER_CONNECTING = 1000,
    SERVER_CONNECTION_ERROR,
    SERVER_CONNECTED, /* Ready to register */
    SERVER_REGISTERING = 2000,
    SERVER_REGISTRATION_ERROR,
    SERVER_REGISTERED, /* Ready to call a peer */
    SERVER_REGISTERING_2 = 2500,
    SERVER_REGISTRATION_ERROR_2,
    SERVER_REGISTERED_2, /* Ready to call a peer */
    SERVER_REGISTERING_3 = 2700,
    SERVER_REGISTRATION_ERROR_3,
    SERVER_REGISTERED_3, /* Ready to call a peer */
    SERVER_CLOSED, /* server connection closed by us or the server */
    PEER_CONNECTING = 3000,
    PEER_CONNECTION_ERROR,
    PEER_CONNECTED,
    PEER_CALL_NEGOTIATING = 3500,
    PEER_CALL_NEGOTIATED = 4000,
    PEER_CALL_STARTED,
    PEER_CALL_STOPPING,
    PEER_CALL_STOPPED,
    PEER_CALL_ERROR,
};
static enum AppState app_state = APP_STATE_UNKNOWN;
#define JANUS_RX_BUFFER_BYTES (1024*100)
enum protocols
{
    PROTOCOL_JANUS = 0,
    PROTOCOL_COUNT
};

static int lws_websocket_connection_send_text(struct lws* wsi_in, char* str, enum MsgType msgtype) {
    if (str == NULL || wsi_in == NULL)
        return -1;
    unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + JANUS_RX_BUFFER_BYTES + LWS_SEND_BUFFER_POST_PADDING];
    unsigned char* p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
    size_t n;

    switch (msgtype) {
    case JANUS_MSS_ICE_CANDIDATE_GATHERED:
        n = sprintf((char*)p, "{\"janus\":\"trickle\",\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld,\"candidate\":{\"completed\":true}}", rand_string(transaction, 12), session_id, handle_id/*, str*/);
        printf("sent %s\n", (char*)p);
        lws_write(wsi_in, p, n, LWS_WRITE_TEXT);
        break;
    case JANUS_MSS_ICE_CANDIDATE:
        n = sprintf((char*)p, "{\"janus\":\"trickle\",\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld,\"candidate\":%s}", rand_string(transaction, 12), session_id, handle_id, str);
        ("sent %s\n", (char*)p);
        lws_write(wsi_in, p, n, LWS_WRITE_TEXT);
        break;
    case JANUS_MSS_SDP_OFFER:
//        n = sprintf((char*)p, "{\"janus\":\"message\",\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld,\"body\":{\"request\":\"configure\",\"audio\":true,\"video\":true},\"jsep\":%s}", rand_string(transaction, 12), session_id, handle_id, str);
        n = sprintf((char*)p, "%s", str);
        printf("sent %s\n", (char*)p);
        lws_write(wsi_in, p, n, LWS_WRITE_TEXT);
        break;
    case JANUS_MSS_SDP_ANSWER:
        n = sprintf((char*)p, "{\"janus\":\"message\",\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld,\"body\":{\"request\":\"configure\",\"audio\":true,\"video\":true},\"jsep\":%s}", rand_string(transaction, 12), session_id, handle_id, str);
        printf("sent %s\n", (char*)p);
        lws_write(wsi_in, p, n, LWS_WRITE_TEXT);
        break;
    case JANUS_MSS_REGISTER:
        n = sprintf((char*)p, "{\"janus\":\"attach\",\"transaction\":\"%s\",\"plugin\":\"janus.plugin.videoroom\",\"opaque_id\":\"videoroomtest-wBYXgNGJGa11\",\"session_id\":%lld}", rand_string(transaction, 12), session_id);
        printf("sent %s\n", (char*)p);
        lws_write(wsi_in, p, n, LWS_WRITE_TEXT);
        break;
    case JANUS_MSS_REGISTER_WITH_SERVER:
        n = sprintf((char*)p, "{\"janus\":\"create\",\"transaction\":\"%s\"}", rand_string(transaction, 12));
        printf("sent %s\n", (char*)p);
        lws_write(wsi_in, p, n, LWS_WRITE_TEXT);
        break;
    case JANUS_MSS_ROOM_REQUEST:
        n = sprintf((char*)p, "{\"janus\":\"message\",\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld,\"body\":{\"request\":\"join\",\"room\":%s,\"ptype\":\"publisher\",\"display\":\"%s\",\"pin\":\"%s\"}}", rand_string(transaction, 12), session_id, handle_id, room, peer_id, token);
        printf("sent %s\n", (char*)p);
        lws_write(wsi_in, p, n, LWS_WRITE_TEXT);
        break;
    case JANUS_MSS_KEEP_ALIVE:
        n = sprintf((char*)p, "{\"janus\":\"keepalive\",\"transaction\":\"%s\",\"session_id\":%lld}", rand_string(transaction, 12), session_id);
        printf("sent %s\n", (char*)p);
        lws_write(wsi_in, p, n, LWS_WRITE_TEXT);
        break;
        ///////////////////////////////////////////////////////////////////
    case JANUS_MSS_ROOM_PARTICIPANTS:
        n = sprintf((char*)p, "{\"janus\":\"message\",\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld,\"body\":{\"request\" : \"listparticipants\",\"room\":%s}}", rand_string(transaction, 12), session_id, handle_id, room);
        printf("sent %s\n", (char*)p);
        lws_write(wsi_in, p, n, LWS_WRITE_TEXT);
        break;
        ///////////////////////////////////////////////////////////////////
    case JANUS_MSS_ROOM_CALL:
        //        if (already == 1) {
        n = sprintf((char*)p, "{\"janus\":\"message\",\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld,\"body\":{\"request\":\"join\",\"room\":%s,\"ptype\":\"subscriber\",\"feed\":%lld,\"pin\":\"%s\"}}", rand_string(transaction, 12), session_id, handle_id, room, feed_id, token);
        //			n = sprintf((char*)p, "{\"janus\":\"message\",\"transaction\":\"%s\",\"session_id\":%"G_GINT64_FORMAT",\"handle_id\":%"G_GINT64_FORMAT",\"body\":{\"request\":\"join\",\"room\":%s,\"ptype\":\"subscriber\",\"feed\":%"G_GINT64_FORMAT",\"private_id\":%s,\"pin\":\"%s\"}}", rand_string(transaction, 12), session_id, handle_id, room, feed_id, rand_string(transaction, 9), token);
        printf("sent %s\n", (char*)p);
        lws_write(wsi_in, p, n, LWS_WRITE_TEXT);
        //        }
        break;
    default:
        break;
    }
    return n;
}

static gboolean
do_register(void)
{
    app_state = SERVER_REGISTERING_2;
    g_print("ATTACH JANUS PLUGIN MSS_REGISTER\n");
    lws_websocket_connection_send_text(web_socket, (char*)"", JANUS_MSS_REGISTER);

    return TRUE;
}

static int websocket_write_back(struct lws* wsi_in, char* str, int str_size_in) {
    if (str == NULL || wsi_in == NULL)
        return -1;
    int n = 0;
    JsonNode* root;
    JsonObject* object;
    JsonParser* parser = json_parser_new();
    if (!json_parser_load_from_data(parser, (char*)str, -1, NULL)) {
        printf("Unknown message '%s', ignoring", (char*)str);
    }
    root = json_parser_get_root(parser);
    if (!root) {
        printf("Error\n");
    }
    object = json_node_get_object(root);
    if (!json_object_has_member(object, "id") || ! json_object_has_member(object, "type")) {
        g_print("id/request field is required!\n");
        return 0;
    }
    const char *type = json_object_get_string_member(object, "type");
    const char *id = json_object_get_string_member(object,"id");
     PeerConnectionState state;
     state = peer_connection_get_state(g_ps.pc);
    if (strcmp(type, "request") == 0) {
        g_print("Receive an new request: %s\n",id);
         if (state == PEER_CONNECTION_CLOSED ||
                 state == PEER_CONNECTION_NEW ||
                 state == PEER_CONNECTION_FAILED ||
                 state == PEER_CONNECTION_DISCONNECTED ) {
             g_ps.id = strdup(id);
             peer_connection_create_offer(g_ps.pc);
         }
    } else if (strcmp(type, "answer") == 0) {
        char * sdp = json_object_get_string_member(object, "sdp");
        g_print("Receive an answer sdp: %s\n", sdp);
        if (state == PEER_CONNECTION_NEW) {

            peer_connection_set_remote_description(g_ps.pc, sdp);
        }
    }
    g_object_unref(parser);

    return n;
}

static int callback_janus(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
    printf("\t START: callback_janus %d\n", reason);
    switch (reason)
    {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
    {
        lws_callback_on_writable(wsi);
        printf("Connected\n");
        break;
    }
    case LWS_CALLBACK_CLIENT_RECEIVE:
    {
        // Handle incomming messages here
        printf("got \n");
        unsigned char* buf = (unsigned char*)
                malloc(LWS_SEND_BUFFER_PRE_PADDING + len
                       + LWS_SEND_BUFFER_POST_PADDING);
        unsigned int i;
        for (i = 0; i < len; i++) {
            buf[LWS_SEND_BUFFER_PRE_PADDING + (len - 1) - i] =
                    ((char*)in)[i];
        }
        printf("received data: %s\n", (char*)in);

        websocket_write_back(wsi, (char*)in, -1);

        free(buf);

        break;
    }
    case LWS_CALLBACK_CLIENT_WRITEABLE:
    {
        app_state = SERVER_REGISTERING;
        lws_websocket_connection_send_text(web_socket,(char*)"",JANUS_MSS_REGISTER_WITH_SERVER);
        break;
    }
    case LWS_CALLBACK_CLOSED:
    {
        printf("\n\n \t\t --- CLOSED --- \n\n");
        web_socket = NULL;
        break;
    }
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    {
        printf("\n\n \t\t --- CLIENT CONNECTION ERROR --- \n\n");
        web_socket = NULL;
        break;
    }
    default:
        break;
    }

    //	printf("\t END: callback_janus %d\n", reason);
    return 0;
}

static struct lws_protocols protocols[] =
{
{
    "janus-protocol",
    callback_janus,
    0,
    JANUS_RX_BUFFER_BYTES,
},
{ NULL, NULL, 0, 0 } /* terminator */
};

static void peer_signaling_onicecandidate(char* description, void* userdata) {
    g_print("send offer here:\n%s\n",description);
    JsonObject *jsepOffer = json_object_new();
    json_object_set_string_member(jsepOffer,"id", g_ps.id);
    json_object_set_string_member(jsepOffer,"type","offer");
    //remove host candidate from sdp

    char  sdp_without_host_candidate[5000];
    memset(sdp_without_host_candidate,0,5000);
    char *line = strtok(description,"\n");
    while (line != NULL) {
        if (strstr(line, "typ host") == NULL) {
            strcat(sdp_without_host_candidate, line);
            strcat(sdp_without_host_candidate,"\n");
        }
        line = strtok(NULL, "\n");
    }
    strcpy(description,sdp_without_host_candidate);
     g_print("send modified offer:\n%s\n",description);
    json_object_set_string_member(jsepOffer,"sdp",description);
    char *text = get_string_from_json_object(jsepOffer);
    lws_websocket_connection_send_text(web_socket, text, JANUS_MSS_SDP_OFFER);

    g_free(text);
}

int peer_signaling_loop() {
    connect_to_janus_server();
    return 0;
}

void peer_signaling_leave_channel() {
    disconnect_websocket();
}

void peer_signaling_set_config(ServiceConfiguration* service_config) {
    char* pos;

    memset(&g_ps, 0, sizeof(g_ps));

    //congnv
    do {
        if (service_config->ws_url == NULL || strlen(service_config->ws_url) == 0) {
            break;
        }

        strncpy(g_ps.ws_host, service_config->ws_url, HOST_LEN);
        g_ps.ws_port = service_config->ws_port;
        LOGI("WS Host: %s, Port: %d", g_ps.ws_host, g_ps.ws_port);
    } while (0);

    // Username, password, id
    if (service_config->client_id != NULL && strlen(service_config->client_id) > 0) {
        strncpy(g_ps.client_id, service_config->client_id, CRED_LEN);
    }

    if (service_config->username != NULL && strlen(service_config->username) > 0) {
        strncpy(g_ps.username, service_config->username, CRED_LEN);
    }

    if (service_config->password != NULL && strlen(service_config->password) > 0) {
        strncpy(g_ps.password, service_config->password, CRED_LEN);
    }

    g_ps.pc = service_config->pc;
    peer_connection_onicecandidate(g_ps.pc, peer_signaling_onicecandidate);
}
#endif  // DISABLE_PEER_SIGNALING

void disconnect_websocket() {
    mainloop = 0;  // Set flag to interrupt the loop
    if (web_socket) {
        lws_callback_on_writable(web_socket);  // Trigger writable callback for closure
    }
}

void connect_to_janus_server()
{
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;

    struct lws_context* context_lw = lws_create_context(&info);

    if (!context_lw) {
        lwsl_err("lws init failed!\n");
        return;
    }

    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0 , sizeof(ccinfo));
    ccinfo.context = context_lw;
    ccinfo.address = g_ps.ws_host;
    //            ccinfo.port = 8188;
    ccinfo.port = g_ps.ws_port;
    ccinfo.path = "/server";
    ccinfo.host = lws_canonical_hostname(context_lw);
    ccinfo.origin = "origin";
    ccinfo.protocol = protocols[PROTOCOL_JANUS].name;
    web_socket = lws_client_connect_via_info(&ccinfo);

    app_state = SERVER_CONNECTING;

    while (mainloop) {
        lws_service(context_lw, /* timeout_ms = */ 250);
    }

    lws_context_destroy(context_lw);
}
