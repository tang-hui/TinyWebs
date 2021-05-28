#include "http_conn.h"

// 定义HTTP响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

// 网站的根目录
const char* doc_root = "/home/huitang/study1/webserver/resources";


// 所有的客户数
int http_conn::m_user_count = 0;
// 所有socket上的事件都被注册到同一个epoll内核事件中，所以设置成静态的
int http_conn::m_epollfd = -1;


int setnoblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 向epoll中添加需要监听的文件描述符
void addfd(int epollfd, int fd,bool one_shot){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;    //EPOLLRDHUP:对端断开连接
    if(one_shot){
        // 防止同一个通信被不同的线程处理
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    //设置文件描述符为非阻塞
    setnoblocking(fd);
}

// 从epoll中移除监听的文件描述符
void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    //printf("close client sockfd: %d\n",fd);
    close(fd);
}

// 修改文件描述符，重置socket上的EPOLLONESHOT事件，以确保下一次可读时，EPOLLIN事件能被触发
void modfd(int epollfd, int fd, int ev){
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}


//关闭连接
void http_conn::close_conn(){
    if(m_sockfd != -1){
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        --m_user_count;
    }
}

void http_conn::init(int sockfd, const sockaddr_in& addr){
    m_sockfd = sockfd;
    m_address = addr;
    // 端口复用
    int reuse = 1;
    setsockopt( m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
    addfd(m_epollfd, m_sockfd, true);
    ++m_user_count;
    init();
}

void http_conn::init(){
    m_check_state = CHECK_STATE_REQUESTLINE;    //初始状态为检查请求行
    m_linger = false;                           //默认不保持长连接
    m_method = GET;                             //默认请求方式为GET
    m_url = 0;
    //m_default_url = "/index.html"; 
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;                           //正在解析的当前请求行的首地址
    m_checked_idx = 0;                          //当前正在分析的字符在缓冲区中的位置
    m_read_idx = 0;                             //标识读取客户端数据的起始位置
    m_write_idx = 0;                            //写缓冲区中待发送的字节数
    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, WRITE_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_LEN);
}

//将HTTP消息一次性读取到读缓冲区m_read_idx中
bool http_conn::read(){
    if( m_read_idx >= READ_BUFFER_SIZE ){  //读取内容大于缓存区大小
        return false;
    }
    int bytes_read = 0;
    while(true){
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if(bytes_read == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){ //没有数据
                //#define EWOULDBLOCK EAGAIN：EWOULDBLOCK 等于 EAGAIN
                //含义：“Resource temporarily unavailable.” The call might work if you try again later.
                break;
            }
            return false;
        }
        else if(bytes_read == 0){  //对方关闭连接
            return false;
        }
        m_read_idx += bytes_read;
    }
    printf("读取到了数据：%s\n ",m_read_buf);
    return true;
}


//解析一行的内容，判断依据\r\n
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    for( ; m_checked_idx < m_read_idx; ++m_checked_idx){
        temp = m_read_buf[ m_checked_idx ];
        if( temp == '\r' ){
            if( m_checked_idx + 1 == m_read_idx ){
                return LINE_OPEN;
            }
            else if( m_read_buf[ m_checked_idx + 1 ] == '\n' ){
                m_read_buf[ m_checked_idx ] = '\0';
                m_read_buf[ m_checked_idx + 1 ] = '\0';
                m_checked_idx += 2;
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if( temp == '\n' ){
            if( m_checked_idx > 1 && m_read_buf[ m_checked_idx - 1 ] == '\r' ){
                m_read_buf[ m_checked_idx - 1 ] = '\0';
                m_read_buf[ m_checked_idx ] = '\0';
                ++m_checked_idx;
                return LINE_OK; 
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 解析HTTP请求行，获取其中的请求方法、目标URL、HTTP版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char * text){
    //参见的HTTP请求行格式: GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t");   //strpbrk:获取text中包含" \t"中任意字符的子串首起始地址,m_url: /index.html HTTP/1.1
    *m_url = '\0';              //text:GET\0/index.html HTTP/1.1
    ++m_url;                    //m_url:/index.html HTTP/1.1
    
    char * method = text;       //method：GET\0
    if( strcasecmp(method, "GET") == 0 ){   //strcasecmp：忽略大小的比较
        m_method = GET;
    }
    else{
        //暂时没有添加请他请求类型（POST、PUT等）的处理方案
        return BAD_REQUEST;
    }

    //m_url:/index.html HTTP/1.1
    m_version = strpbrk( m_url, " \t");
    if( !m_version ){
        return BAD_REQUEST;
    }
    *m_version = '\0';      //m_url:/index.html\0
    ++m_version;            //m_version:HTTP/1.1
    if( strcasecmp(m_version, "HTTP/1.1") != 0 ){
        return BAD_REQUEST; //暂时只处理HTTP1.1版本
    }

    //有时候m_url的格式为:http://192.168.110.129:10000/index.html\0
    if( strncasecmp(m_url, "http://", 7) == 0 ){
        m_url += 7;                     //丢弃开头的http://
        m_url = strchr( m_url, '/' );   //查找第一次出现‘/’的位置，m_url:/index.html\0
    }
    if( !m_url || m_url[0] != '/' ){
        return BAD_REQUEST;
    }
    if( strcmp(m_url,"/") == 0 ){
        m_url = (char *)m_default_url;
    }

    m_check_state = CHECK_STATE_HEADER;  //检查状态变为检查头
    return NO_REQUEST;
}


//解析HTTP请求的头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char * text){
    if( text[0] == '\0' ){
        //如果请求体的长度不为0，则需要将检查状态变为检查消息体
        if(m_content_length != 0){  
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if( strncasecmp(text, "Connection:", 11) == 0 ){    //处理connection头部字段
        //常见格式：Connection: keep-alive
        text += 11;
        text += strspn(text, " \t");        //排除"Connection:"之后的空格
        if( strcasecmp(text, "keep-alive" ) == 0 ){
            m_linger = true;
        }
    }
    else if( strncasecmp(text, "Content-Length", 15) == 0 ){
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if( strncasecmp(text, "Host:", 5) == 0 ){
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else{
        //暂时只处理以上几种基本的头部信息，其他的后续再补充
        printf( "oop! unknow header %s\n", text );
    }
    return NO_REQUEST;
}

//暂时没有实现对消息体内容的处理，目前知识判断是否完整的读入了消息体
http_conn::HTTP_CODE http_conn::parse_content(char * text){
    if( m_read_idx >= (m_content_length + m_checked_idx) ){
        text[ m_content_length ] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

//主状态机，解析请求
http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char * text = 0;
    while( (m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) 
            || ( (line_status = parse_line()) == LINE_OK) ){
        
        text = get_line();  //获取一行数据
        m_start_line = m_checked_idx;
        printf("sucess got 1 http line:%s\n", text);
        switch (m_check_state){
            case CHECK_STATE_REQUESTLINE:{
                ret = parse_request_line( text );
                if( ret == BAD_REQUEST ){
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:{
                ret = parse_headers(text);
                if( ret == BAD_REQUEST ){
                    return BAD_REQUEST;
                }
                else if( ret == GET_REQUEST ){
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:{
                ret = parse_content( text );
                if ( ret == GET_REQUEST ) {
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default:{
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}


// 当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性，
// 如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其
// 映射到内存地址m_file_address处，并告诉调用者获取文件成功
http_conn::HTTP_CODE http_conn::do_request(){
    //m_real_file = doc_root + m_url
    strcpy( m_real_file, doc_root );
    int len = strlen(doc_root);
    strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );

    //获取客户请求的目标文件的状态信息
    if( stat(m_real_file, &m_file_stat) != 0){
        return NO_RESOURCE;
    }

    //判断是否有访问权限
    if( !(m_file_stat.st_mode & S_IROTH) ){
        return FORBIDDEN_REQUEST;
    }

    //判断是否为根目录
    if( S_ISDIR( m_file_stat.st_mode ) ){
        return BAD_REQUEST;
    }

    //以只读方式打开文件
    int fd = open( m_real_file, O_RDONLY );

    //创建内存映射
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

//对内存映射区域执行munmap操作
void http_conn::unmap(){
    if( !m_file_address ){
        munmap( m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

//写HTTP响应
bool http_conn::write(){
    int temp = 0;
    int bytes_have_send = 0;            //已经发送的字节
    int bytes_to_send = m_write_idx;    //将要发送的字节

    if( bytes_to_send == 0 ){
        //将要发送的字节为0，本次响应结束
        modfd( m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while(1){
        //分散写
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if( temp <= -1){
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
            if( errno == EAGAIN ){
                modfd( m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        //bytes_to_send -= temp;
        bytes_have_send += temp;
        if( bytes_have_send >= bytes_to_send){
            // 发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接
            unmap();
            if(m_linger){
                init();
                modfd( m_epollfd, m_sockfd, EPOLLIN);
                return true;
            }
            else{
                //printf("don't keep alive\n");
                modfd( m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}


//往写缓冲区中写入待发送的数据
bool http_conn::add_response( const char * format, ... ){
    if(m_write_idx >= WRITE_BUFFER_SIZE){
        printf("写入发送缓冲区的数据过长\n ");
        return false;
    }
    va_list arg_list;               //指向当前参数的指针
    va_start( arg_list, format );   //使arg_list指向第一个参数
    //将输入的参数格式化输入到写缓冲区中，若输入的参数长度大于指定长度，则返回参数的长度，并指拷贝指定长度的内容
    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if( len > ( WRITE_BUFFER_SIZE - 1 - m_write_idx) ){    //判断输入的参数是否大于写缓冲区的长度
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );             //关闭arg_list指针
    return true;
}


bool http_conn::add_status_line( int status, const char * title){
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len){
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int content_length){
    return add_response( "Content-Length: %d\r\n", content_length);
}

bool http_conn::add_content_type(){
    return add_response( "Content-Type:%s\r\n", "text/html");
}

bool http_conn::add_linger(){
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}

bool http_conn::add_blank_line(){
    return add_response( "%s", "\r\n");
}

bool http_conn::add_content( const char* content ) 
{
    return add_response( "%s", content );
}


//根据服务器处理HTTP请求的结果，决定返回给客户端的内容
bool http_conn::process_write(HTTP_CODE ret){
    switch (ret){
        case INTERNAL_ERROR:
            add_status_line( 500, error_500_title);
            add_headers( strlen( error_500_form) );
            if( !add_content( error_500_form ) ){
                return false;
            }
            break;
        case BAD_REQUEST:
            add_status_line( 400, error_400_title );
            add_headers( strlen( error_400_form ) );
            if ( ! add_content( error_400_form ) ) {
                return false;
            }
            break;
        case NO_RESOURCE:
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) ) {
                return false;
            }
            break;
        case FORBIDDEN_REQUEST:
            add_status_line( 403, error_403_title );
            add_headers(strlen( error_403_form));
            if ( ! add_content( error_403_form ) ) {
                return false;
            }
            break;
        case FILE_REQUEST:
            add_status_line(200, ok_200_title );
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            return true;
        default:
            return false;
    }

    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}


//由线程池中的工作线程调用，这是处理HTTP请求的入口函数
void http_conn::process(){
    //解析HTTP请求
    HTTP_CODE read_ret = process_read();
    if( read_ret == NO_REQUEST ){
        printf("process_read failue!\n ");
        modfd( m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    //生成响应
    bool write_ret = process_write( read_ret );
    if( !write_ret ){
        close_conn();
    }
    modfd( m_epollfd, m_sockfd, EPOLLOUT);  //ET模式下EPOLLOUT事件只有在写缓存区从不可写转换到可以写的状态下会触发，但如果重置EPOLLOUT事件会立马触发一次
}

