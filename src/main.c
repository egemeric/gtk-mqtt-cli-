#ifdef WIN32
#undef UNICODE
#endif
#if !defined(_WIN32)
#include <unistd.h>
#else
#include <windows.h>
#endif

#if defined(_WRS_KERNEL)
#include <OsWrapper.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include "MQTTAsync.h"

#define ADDRESS "tcp://10.1.1.2:1883"
#define CLIENTID "Client01"
#define TOPIC "#"
#define QOS 1
#define TIMEOUT 10000L
MQTTAsync client;
MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
GtkWidget *log;
char msg_buff[256];
int ct = 0;
int disc_finished = 0;
int subscribed = 0;
int finished = 0;

void onConnect(void *context, MQTTAsync_successData *response);
void onConnectFailure(void *context, MQTTAsync_failureData *response);
void add_log(char *text)
{
    ct++;
    GtkTextIter iter;
    GtkTextBuffer *buffer;
    GtkTextIter startIter, endIter;
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log));
    if (ct > 10)
    {
        gtk_text_buffer_get_iter_at_line(buffer, &startIter, 15);
        gtk_text_buffer_get_iter_at_line(buffer, &endIter, 16);
        gtk_text_buffer_delete(buffer, &startIter, &endIter);
    }

    gtk_text_buffer_get_iter_at_line(buffer, &iter, 0);
    char line[256];
    sprintf(line, "%s[%d]\n", text, ct);
    gtk_text_buffer_insert(buffer, &iter, line, -1);
}
void connlost(void *context, char *cause)
{
    MQTTAsync client = (MQTTAsync)context;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    int rc;

    printf("\nConnection lost\n");
    if (cause)
        printf("     cause: %s\n", cause);

    printf("Reconnecting\n");
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = onConnect;
    conn_opts.onFailure = onConnectFailure;
    if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to start connect, return code %d\n", rc);
        finished = 1;
    }
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    sprintf(msg_buff, "Topic:%s Message: [%.*s]", topicName, message->payloadlen, (char *)message->payload);
    add_log(msg_buff);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void onDisconnectFailure(void *context, MQTTAsync_failureData *response)
{
    printf("Disconnect failed, rc %d\n", response->code);
    disc_finished = 1;
}

void onDisconnect(void *context, MQTTAsync_successData *response)
{
    printf("Successful disconnection\n");
    disc_finished = 1;
}

void onSubscribe(void *context, MQTTAsync_successData *response)
{
    printf("Subscribe succeeded\n");
    subscribed = 1;
}

void onSubscribeFailure(void *context, MQTTAsync_failureData *response)
{
    printf("Subscribe failed, rc %d\n", response->code);
    finished = 1;
}

void onConnectFailure(void *context, MQTTAsync_failureData *response)
{
    printf("Connect failed, rc %d\n", response->code);
    finished = 1;
}

void onConnect(void *context, MQTTAsync_successData *response)
{
    MQTTAsync client = (MQTTAsync)context;
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    int rc;

    add_log("Successful connection");

    sprintf(msg_buff, "Subscribing to topic %s\nfor client %s using QoS%d", TOPIC, CLIENTID, QOS);
    add_log(msg_buff);
    opts.onSuccess = onSubscribe;
    opts.onFailure = onSubscribeFailure;
    opts.context = client;
    if ((rc = MQTTAsync_subscribe(client, TOPIC, QOS, &opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to start subscribe, return code %d\n", rc);
        finished = 1;
    }
}

void mqtt_connect(GtkButton *connect, gpointer data)
{

    add_log("button is clicked");
    int rc;
    if ((rc = MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to create client, return code %d\n", rc);
        rc = EXIT_FAILURE;
    }

    if ((rc = MQTTAsync_setCallbacks(client, client, connlost, msgarrvd, NULL)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to set callbacks, return code %d\n", rc);
        rc = EXIT_FAILURE;
    }

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = onConnect;
    conn_opts.onFailure = onConnectFailure;
    conn_opts.context = client;
    if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to start connect, return code %d\n", rc);
        rc = EXIT_FAILURE;
    }
}

static void signal_handler(int sig)
{
    printf("EXITTING\n");
    MQTTAsync_destroy(&client);
    sleep(1);
    exit(0);
}

int main(int argc, char *argv[])
{

    signal(SIGINT, signal_handler);
    GtkWidget *window;
    GtkBuilder *builder;
    GError *err = NULL;
    gtk_init(&argc, &argv);
    builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "src/mqtt_app.glade", &err);
    window = GTK_WIDGET(gtk_builder_get_object(builder, "mqtt_app"));
    gtk_builder_connect_signals(builder, NULL);
    gtk_widget_show(window);
    g_signal_connect(window, "destroy", G_CALLBACK(signal_handler), NULL);
    log = GTK_WIDGET(gtk_builder_get_object(builder, "log"));
    gtk_main();
}
