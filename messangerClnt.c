#include <gtk/gtk.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_BUFFER_SIZE 1024

typedef struct _Data Data;

struct _Data {
    GtkWidget *window;
    GtkWidget *entry;
    GtkWidget *textView;
    GtkWidget *sendMessageButton;
    GtkWidget *sendFileButton;
    int sockfd;
};

G_MODULE_EXPORT void quit(GtkWidget *window, gpointer data) {
    close(((Data*)data)->sockfd);
    gtk_main_quit();
}

G_MODULE_EXPORT void send_message_button_clicked(GtkButton *button, Data *data) {
    GtkTextBuffer *buffer;
    GtkTextMark *mark;
    GtkTextIter iter;
    const gchar *message;

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->textView));

    if (buffer == NULL || !GTK_IS_TEXT_BUFFER(buffer)) {
        g_print("Error: Text buffer is not initialized or invalid.\n");
        return;
    }

    message = gtk_entry_get_text(GTK_ENTRY(data->entry));
    mark = gtk_text_buffer_get_insert(buffer);
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

    if (gtk_text_buffer_get_char_count(buffer))
        gtk_text_buffer_insert(buffer, &iter, "\n", 1);

    // 내가 보내는 메시지 tag로 우측 정렬
    GtkTextTag *tag = gtk_text_buffer_get_tag_table(buffer);
    if (!gtk_text_tag_table_lookup(tag, "right_alignment")) {
        tag = gtk_text_buffer_create_tag(buffer, "right_alignment", "justification", GTK_JUSTIFY_RIGHT, NULL);
    }

    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, message, -1, "right_alignment", NULL);

    mark = gtk_text_buffer_get_insert(buffer);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(data->textView), mark);

    // 서버에 메시지 전송
    if (send(data->sockfd, message, strlen(message), 0) == -1) {
        perror("send error");
        exit(1);
    }

    gtk_entry_set_text(data->entry, "");
}

G_MODULE_EXPORT void send_file_button_clicked(GtkButton *button, Data *data) {
    // 파일 선택 다이얼로그 생성
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Select a file", GTK_WINDOW(data->window), GTK_FILE_CHOOSER_ACTION_OPEN, 
                                                        "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);

    // 파일 형식 필터 추가 (선택적)
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "All Files");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    // 사용자가 "Open"을 선택하면
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        // 선택한 파일의 경로 얻기
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        // 파일 전송 함수 호출
        send_file(data->sockfd, filename);

        // 메모리 해제 및 다이얼로그 닫기
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

void send_file(int sockfd, const char *filename) {
    // 파일 열기
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("File open error");
        return;
    }

    // 파일 전송을 알리고 파일 이름 전송
    char fileMessage[262] = "<file>";
    strcat(fileMessage, filename);
    send(sockfd, fileMessage, strlen(fileMessage), 0);

    char buffer[MAX_BUFFER_SIZE];
    size_t bytesRead;

    // 파일 내용을 버퍼에 읽어와 전송
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(sockfd, buffer, bytesRead, 0);
    }
    send(sockfd, "<EOF>", 5, 0);

    fclose(file);
}

void receive_data(Data *data) {
    fd_set readfds, activefds;
    FD_ZERO(&activefds);

    FD_SET(data->sockfd, &activefds);

    int maxfd = data->sockfd;

    struct timeval timeout;
    timeout.tv_sec = 1;  // 타임아웃을 1초로 설정
    timeout.tv_usec = 0;

    while(1) {
        readfds = activefds;
        int n;
        if ((n = select(maxfd + 1, &readfds, NULL, NULL, &timeout)) < 0) {
            perror("select error");
            exit(1);
        }

        if (FD_ISSET(data->sockfd, &readfds)) {
            // 서버로부터 메시지를 받았을 때
            char msg_recv[1024];
            if ((n = recv(data->sockfd, msg_recv, sizeof(msg_recv), 0)) < 0) {
                perror("receive error");
                exit(1);
            }

            if (n == 0) {
                printf("server closed\n");
                break;
            }

            msg_recv[n] = '\0';

            // textView에 전송받은 메시지 출력
            GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->textView));
            GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
            GtkTextIter iter;
            gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

            if (gtk_text_buffer_get_char_count(buffer))
                gtk_text_buffer_insert(buffer, &iter, "\n", 1);

            gtk_text_buffer_insert(buffer, &iter, msg_recv, -1);
            mark = gtk_text_buffer_get_insert(buffer);
            gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(data->textView), mark);
        }
    }
}

int main(int argc, char *argv[]) {
    GtkBuilder *builder;
    GError *error = NULL;
    Data *data;
    gtk_init(&argc, &argv);

    int sockfd;
    struct sockaddr_in serv_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "failed to create socket\n");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_aton("127.0.0.1", &(serv_addr.sin_addr));

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) < 0) {
        fprintf(stderr, "failed to connect to server\n");
        exit(1);
    }

    // 빌더 생성 및 UI 파일 열기
    builder = gtk_builder_new();
    if (!gtk_builder_add_from_file(builder, "messangerClient.xml", &error)) {
        g_print("UI 파일을 읽을 때 오류 발생!\n");
        g_print("메시지: %s\n", error->message);
        g_free(error);
        return 1;
    }

    data = g_slice_new(Data);
    data->window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    data->sendMessageButton = GTK_WIDGET(gtk_builder_get_object(builder, "sendMessageButton"));
    data->sendFileButton = GTK_WIDGET(gtk_builder_get_object(builder, "sendFileButton"));
    data->entry = GTK_WIDGET(gtk_builder_get_object(builder, "entry"));
    data->textView = GTK_WIDGET(gtk_builder_get_object(builder, "textView"));
    
    data->sockfd = sockfd;

    gtk_window_set_default_size(GTK_WINDOW(data->window), 500, 400);


    // 시그널 연결
    gtk_builder_connect_signals(builder, data);
    g_object_unref(G_OBJECT(builder));
    gtk_widget_show_all(data->window);

    // 메시지 전송받기 위한 쓰레드
    GThread *thread = g_thread_new("receive_data_thread", (GThreadFunc)receive_data, data);

    gtk_main();

    g_thread_join(thread);

    close(sockfd);
    g_slice_free(Data, data);

    return 0;
}
