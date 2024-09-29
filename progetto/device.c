#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

char buff[200];
uint8_t vett[512]; // per file_sharing
int sd, p2p, ret, dim;   //socket connesso al server - socket p2p(listener) accetta le connessioni da altri device - variabile per verificare eventuali errori - variabile di appoggio
int server_status;         //bool che mi dice se server online(1) o offline(0)
uint16_t len;

int LOGIN();
int LogSig();
int SIGNUP();
void menu();
void HANGING();
void SHOW();
int RUBRICAcontrol(char utente[]);
void CRONOLOGIA(char utente[]);
void CHAT(char utente[]);
void showLIST();
int INVIO(char utente[]);
void RUBRICA();
void OUT();
int checkfile(char file_name[],int *num_bytes);
void SHAREfile(char file_name[],int num_bytes,int socket);
void RECEIVEfile(int socket);

struct device{
    char user[25];
    char passwd[25];
    int port;
    int req;    //1 SIGNUP 2LOGIN
} d;

struct chat{
    char user[25];
    int socket;
    struct chat* punt;
};
struct chat* head;

fd_set master; // globale perchè anche la funzione chat ci lavora


int main(int argc, char** argv){

    char utente[25]; //serve per memorizzare utente con cui si vuole chattare

    fd_set work;
    int fdmax;
    int i, opt, new; // new = nuovo socket per connessione con altro device, socket p2p socket che accetta le connessioni da altri device

    struct chat* p,*q;      // per gestire lista delle chat (inserimenti e eliminazioni)

    struct sockaddr_in server,client,deviceClient,deviceServer;
    // head == NULL;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0){
        printf("Errore creazione socket");
        exit(-1);
    }

    p2p = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0){
        printf("Errore creazione socket");
        exit(-1);
    }

    d.port = 4243;      //porta di default
    if ( argc > 1 )
        sscanf(argv[1],"%d",&d.port);

    FD_ZERO(&master);
    FD_ZERO(&work);
            

    memset(&server,0,sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(4242);
    inet_pton(AF_INET, "localhost",&server.sin_addr);

    memset(&client,0,sizeof(client));
    client.sin_family = AF_INET;
    client.sin_port = htons(d.port);
    inet_pton(AF_INET, "localhost",&client.sin_addr);

    memset(&deviceServer,0,sizeof(deviceServer));
    deviceServer.sin_family = AF_INET;
    deviceServer.sin_port = htons(d.port+100);
    inet_pton(AF_INET, "localhost",&deviceServer.sin_addr);

    bind(sd, (struct sockaddr*)&client, sizeof(client));

    bind(p2p, (struct sockaddr*)&deviceServer, sizeof(deviceServer));

    listen(p2p,5); 

    ret = connect(sd,(struct sockaddr*)&server,sizeof(server));
    if(ret < 0){
        printf("ERRORE con la connect\n");
        exit(-1);
    }

    FD_SET(sd,&master);
    FD_SET(STDIN_FILENO,&master);
    fdmax = sd;

    
// ################# ACCENSIONE E MENU SIGNUP - LOGIN ############################

    for(i = 0; i < 30; i++){
        printf("*");
    }
    printf("\nDEVICE STARTED SUCCESSFULLY\n");
    for(i = 0; i < 30; i++){
        printf("-");
    }
    sleep(1);
    printf("\nSalve, per iniziare a messaggiare prima devi autenticarti:\n");
    printf("1) SIGNUP -----> iscriviti alla nostra piattaforma\n");
    printf("2) LOGIN ------> effettuare l'accesso, solo per utenti iscritti\n");
    printf("-\n");
    sleep(1);
     
    while ((opt = LogSig())){                // ciclo su login e signup fino a che login non va a buon fine
        
        if (opt == 1){  //SIGNUP            // si puo togliere if e else d.req = opt non lo so con i break
            ret = SIGNUP();
            if (ret){
                printf("\nIscrizione avvenuta con successo,\nLoggarsi per iniziare a messaggiare\n"); //riparto dal menu iniziale per fare login
            }
            else{
                printf("\nIscrizione non riuscita: usrname non disponibile\n");  // si riparte dal menu iniziale
            }
            
        }
        else{
            ret = LOGIN();   // provo LOGIN
            if (ret){
                printf("Login avvenuta con successo\n");
                server_status = 1;     // se ho fatto login con successo server è online
                break;      // entro nel servizio
            }
            else{
                printf("Username e Password sbagliati.\n");  // si riparte dal menu iniziale
            }
        }
    }

//  ###################### MENU #########################
    menu();

    while(1){

        work = master;  // inizialmente solo sd(connessione con server) + stdin
        select(fdmax+1, &work, NULL, NULL, NULL);
        for ( i = 0 ; i <= fdmax ; i++ ){

            if( FD_ISSET(i,&work) ){
//      ######################## GESTIONE MENU DEVICE #################################################
                if (i == STDIN_FILENO){
                    ret = scanf("%d", &opt);
                    if(!ret)    //controllo in caso non venga digitato un numero
                        opt = 8;

                    while('\n'!= getchar()); //pulizia residua di scanf

                    switch(opt){        //se utente non digita un numero?

                        case 1: 
                            HANGING();
                            break;
                        case 2:
                            SHOW();
                            break;

                        case 3:
                            printf("\nDigita lo username dell'utente con il quale vuoi iniziare a chattare\n");
                            scanf("%s",utente);
                            //while('\n'!= getchar()); //pulizia residua di scanf
                            ret = RUBRICAcontrol(utente); // controllo esistenza utente in rubrica e nel caso mostro cronologia
                            
                            if (!ret){ // utente non esiste o non in rubrica
                                printf("Utente non presente in rubrica, Impossibile aprire chat.\n\n");
                                break;
                            }
                            // utente presente in rubrica
                            // APRI CHAT
                            CRONOLOGIA(utente);
                            
                            // ### ENTRO NELLA CHAT -> loop
                            CHAT(utente);   

                            // ### SONO USCITO DALLA CHAT    
                            p = head;
                            while(p != NULL){   //aggiungo a master tutti i socket aperti con utenti nella chat
                                if(!FD_ISSET(p->socket,&master)){
                                    FD_SET(p->socket,&master);
                                    if(p->socket > fdmax)
                                        fdmax = p->socket;
                                }
                                p = p->punt;
                            }
                            menu();
                            break;
                        
                        case 4:
                            printf("\nPer condividere un file, entrare in una chat (3) e digitare la keyword share seguita dal nome del file\n\n");
                            break;

                        case 5:
                            RUBRICA();
                            break;

                        case 6:
                            OUT();
                            break;

                        default:
                            printf("\nDIGITA INPUT VALIDO\n\n");
                            break;
                    }
                }
//      ########################## GESTIONE MESSAGGI IN ARRIVO #########################################
                else{   

                    if (i == sd){             // ### IL SERVER MI HA INVIATO UN MESSAGGIO
                        /*QUALCUNO STA CERCANDO DI INIZIARE
                            UNA CHAT CON ME - OPPURE MI ARRIVA UN ACK*/
                        
                        // prendo messaggio server (ack o username)
                        ret = recv(sd,&len,sizeof(len),0);
                        if (ret <= 0){
                            printf("\nSERVER DISCONNESSO\n\n");     
                            server_status = 0;      //server offline
                            close(sd);
                            FD_CLR(sd,&master);
                            continue;
                        }
                        dim = ntohs(len);
                        // NUOVO LOGIN CON MIE CREDENZIALI
                        if (dim == 1000){ //caso in cui server mi informa nuovo device si sia loggato con mie credenziali
                            printf("\nSessione scaduta, nuovo login effettuato da altro device\n\n");
                            close(sd);
                            // dealloco tutto prima di uscire (non serve salvare time_stamp perchè tanto c'è altro device loggato)
                            p = head;
                            while (p != NULL ){
                                head = p->punt;
                                close(p->socket);
                                free(p);
                                p = head; 
                            }
                            exit(1);
                        }
                        // ACK QUALCUNO HA LETTO MIEI MESSAGGI PENDENTI
                        if (dim == 1001){ // dim normalmente di nome utente (<=25)
                            ret = recv(sd,&len,sizeof(len),0);
                            if (ret < 0){
                                printf("problemi con reccv per ack notifica\n");
                            }

                            dim = ntohs(len);

                            ret = recv(sd,utente,dim,0);      // in utente è contenuto l'username che ha letto miei messaggi
                            if (ret <= 0){
                                printf("recv di SD ha dato <= 0\n");
                            }

                            printf("\n<%s> ha letto i tuoi messaggi.\n",utente);
                            continue;
                        }
                            
                        
                        // HANNO INIZIATO UNA CHAT CON ME
                        ret = recv(sd,utente,dim,0);      // in utente è contenuto l'username del device che ha iniziato chat
                        if (ret <= 0){
                            printf("recv di SD ha dato <= 0\n");
                        }

                        ret = recv(sd,&len,sizeof(len),0);
                        if (ret <= 0){
                            printf("recv di SD ha dato <= 0\n");
                        }

                        dim = ntohs(len);

                        ret = recv(sd,buff,dim,0);      // in buff è contenuto il messaggio dal device che ha iniziato chat
                        if (ret <= 0){
                            printf("recv di SD ha dato <= 0\n");
                        }
                
                        // creo nuovo socket per connessione con mittente, la connect la fa lui
                        dim = sizeof(deviceClient);
                        new = accept(p2p,(struct sockaddr*)&deviceClient,&dim);
                        if(new <= 0){
                            perror("ERRORE: ");
                            printf("l' utente  %s ha provato a mandarti un messaggio iniziale\n, ma errore di accept.\n",utente);
                            continue;
                        }



                        // creo struct nuova chat 
                        p = (struct chat*)malloc(sizeof(struct chat));
                        strcpy(p->user,utente);
                        p->socket = new;
                        p->punt = NULL;
                        // inserimento in testa alla lista delle chat
                        if(head)
                            p->punt = head;
                        head = p;
                        
                        //aggiungo il socket a quelli da monitorare
                        FD_SET (new, &master);
                        if ( new > fdmax )
                            fdmax = new;

                        // mostro il messaggio
                        printf("\nL'utente <%s> ha iniziato una chat con te.\nMessaggio:%s\n",p->user,buff);
                                     
                    }
                    else{                      // ### GESTISCO MESSAGGI INVIATI DAGLI UTENTI se non sono dentro una chat con loro
                        
                        ret = recv(i,&len,sizeof(len),0);
                        if (ret <= 0){          // ### DEVICE DISCONNESSO OPPURE ERRORE  
                            FD_CLR(i,&master);
                            close(i);
                            if (ret < 0){       // ### CASO ERRORE
                                printf("Errore dovuto alla recv\n");
                                exit(-1);
                            }

                            // ### GESTIRE CASO DISCONNESSIONE DEVICE 
                            // elimino chat dalla lista
                            p = head;
                            while(p != NULL ){
                                if ( p->socket == i ){
                                    if (p == head)
                                        head = p->punt;
                                    else
                                        q->punt = p->punt;
                                    break;
                                }
                                q = p;
                                p = p->punt;
                            } 
                            printf("\nCHIUSA LA CONNESSIONE con %s\n",p->user);
                            free(p);
                            continue;
                        }

                        dim = ntohs(len);
                        // ho ricevuto il messaggio e lo mostro a video

                        ret = recv(i,buff,dim,0);      // in buff è contenuto il messaggio dal device che ha iniziato chat
                        if (ret <= 0){
                            printf("recv di SD ha dato <= 0\n");
                        }

                        // scopro chi è che mi ha mandato il messaggio
                        p = head;
                        while ( p != NULL ){
                            if (p->socket == i)
                                break;
                            p = p->punt;
                        }

                        //FILE SHARE ----------------------------------------
                        if (!strcmp(buff,"share")){
                            printf("L' utente [%s] ti sta inviando un file\n",p->user);
                            RECEIVEfile(p->socket);
                            continue;
                        }
                        // -------------------------------------------------

                        printf("\nE' arrivato un messaggio da <%s>:\n%s\n",p->user,buff);
                    }
                    
                }
            }
        }     

    }

    exit(1);

}

int LogSig(){
    int opt;    //1->signup 2->login
    char credenziali[50];

    printf("\nSIGNUP(1) O LOGIN(2)?\n");      // da qui devi iniziare un ciclo che si ripete fino a autenticaz

    // attendo che utente digiti un comando valido
    while(1){
        scanf("%d",&opt);
        if(opt==1 || opt==2)
            break;
        printf("Digitare un input valido\n");
        while('\n'!= getchar()); //pulizia residua di scanf
    }

    while('\n'!= getchar()); //pulizia residua di scanf

    
    d.passwd[0] = '\0';

    // attendo che scriva due o più parole (usr e psw) quelle in più verranno scartate
    while (d.passwd[0] == '\0'){
        printf("\nScrivi username e password separati da spazio\n");
        fgets(credenziali,50,stdin);
        sscanf(credenziali,"%s %s",d.user,d.passwd);
    }

    return opt;
}

int SIGNUP(){  // fallisce se user inserito è gia presente
    
    
    d.req = 1;  // RICHIESTA LOGIN

    sprintf(buff,"%s %s %d %d",d.user,d.passwd,d.port,d.req);
    dim = strlen(buff)+1;
    len = htons(dim);

    ret = send(sd,&len,sizeof(len),0);
    if (ret <= 0){
        printf("SEND ha resetituito <= 0\n");
        exit(-1);
    }

    ret = send(sd,buff,strlen(buff)+1,0);
    if (ret <= 0){
        printf("SEND ha resetituito <= 0\n");
        exit(-1);
    }

    ret = recv(sd,&len,sizeof(uint16_t),0);
    if (ret <= 0){
        printf("RECV ha resetituito <= 0\n");
        exit(-1);
    }

    dim = ntohs(len);   //esito della signup
    return dim; 

}

int LOGIN(){

    d.req = 2;  // RICHIESTA LOGIN

    sprintf(buff,"%s %s %d %d",d.user,d.passwd,d.port,d.req);
    dim = strlen(buff)+1;
    len = htons(dim);

    ret = send(sd,&len,sizeof(len),0);
    if (ret <= 0){
        printf("SEND ha resetituito <= 0\n");
        exit(-1);
    }

    ret = send(sd,buff,strlen(buff)+1,0);
    if (ret <= 0){
        printf("SEND ha resetituito <= 0\n");
        exit(-1);
    }

    ret = recv(sd,&len,sizeof(uint16_t),0);
    if (ret <= 0){
        printf("RECV ha resetituito <= 0\n");
        exit(-1);
    }

    dim = ntohs(len);  //esito della login
    return dim; 
}

void menu(){
    printf("\nSalve %s,\n",d.user);
    printf("Digita il numero corrispondente al comando che vuoi eseguire:\n");
    printf("1) HANGING -> permette di vedere chi e quando ti ha scritto mentre eri assente.\n");
    printf("2) SHOW -> mostra i messaggi pendenti di un utente.\n");
    printf("3) CHAT -> avvia una chat con uno o più utenti.\n");
    printf("4) SHARE -> condividi file con uno o più utenti.\n");
    printf("5) RUBRICA -> mostra lista degli utenti presenti nella tua rubrica.\n");
    printf("6) OUT -> interrompi sessione e esegui logout.\n");
}

void HANGING(){

    if (!server_status){
        printf("\nServer momentanemanete OFFLINE.\nRiprovare più tardi.\n");
        return;
    }

    d.req = 10;

    //invio informazioni di routine
    sprintf(buff,"%s %s %d %d",d.user,d.passwd,d.port,d.req);
    dim = strlen(buff)+1;
    len = htons(dim);
    ret = send(sd,&len,sizeof(len),0);
    if (ret <= 0){
        server_status = 0;
        return;
    }
    
    ret = send(sd,buff,strlen(buff)+1,0);
    if (ret <= 0){
        printf("SEND ha resetituito <= 0\n");
        exit(-1);
    }
    //ricevo risposta
    ret = recv(sd,&len,sizeof(len),0);
    if(ret <= 0){
        printf("problemi con recv \\u\n");
        exit(1);
    }
    dim = ntohs(len);
    ret = recv(sd,buff,dim,0);
    if(ret <= 0){
        printf("problemi con recv \\u\n");
        exit(1);
    }
    
    //stampo a video
    printf("\n%s\n\n",buff);
}

void SHOW(){
    char utente[25];   //username utente di cui vogliamo fare il check
    char utente_square[25];   // [utente]
    char user_raw[25];  //serve per fare check con utente
    char asterisco[25];
    int conto = 0;
    FILE* chat;

    printf("\nDigita lo username dell'utente di cui vuoi controllare i messaggi non letti\n");
    scanf("%s",utente);

    // check se nome utente è valido
    if(!RUBRICAcontrol(utente)){
        printf("\nlo username inserito non appartiene alla tua rubrica\n");
        return;
    }

    // meccanismo apertura chat
    if(strcmp(d.user,utente) < 0)
        sprintf(buff,"GENERAL/%s+%s-chat.txt",d.user,utente);
    else
        sprintf(buff,"GENERAL/%s+%s-chat.txt",utente,d.user);

    chat = fopen(buff,"r+");
    if(!chat){       // caaso in cui non è ancora iniziata la chat
        printf("Nessun messaggio Pendente da %s\n\n",utente);
        return;
    }

    // costruisco formato [utente]
    sprintf(utente_square,"[%s]",utente);
    

    while( fscanf(chat,"%s",asterisco) != EOF ){
        if (!strcmp(asterisco,"*")){    // se trovi un carattere *
            fseek(chat,strlen(asterisco),SEEK_CUR);
            fscanf(chat,"%s",user_raw);
            if (!strcmp(user_raw,utente_square)){ // se username di quella riga è [utente] 
                fseek(chat,-(strlen(user_raw)+2), SEEK_CUR);
                fprintf(chat," ");
                fgets(buff,200,chat);
                printf("*%s",buff);
                conto++;
            }
            else{
                printf("\nNon ci sono messaggi pendenti\n");
                fclose(chat);
                return;
            }
        }
    }
    fclose(chat);

    if (conto == 0){
        printf("\nNon ci sono messaggi pendenti.\n\n");
        return;
    }
    else
        printf("\nFine dei messaggi pendenti da %s\n\n",utente);

    // Se server è online notifico la lettura dei messaggi pendenti
    if(server_status){
        d.req = 20;

        sprintf(buff,"%s %s %d %d",d.user,d.passwd,d.port,d.req);
        dim = strlen(buff)+1;
        len = htons(dim);
        ret = send(sd,&len,sizeof(len),0);
        if (ret <= 0){
            server_status = 0;
            return;
        }

        ret = send(sd,buff,strlen(buff)+1,0);
        if (ret <= 0){
            printf("SEND ha resetituito <= 0\n");
            exit(-1);
        }
        // invio nome utente a cui notificare la lettura
        sprintf(buff,"%s",utente);
        dim = strlen(buff)+1;
        len = htons(dim);

        ret = send(sd,&len,sizeof(len),0);
        if (ret <= 0){
            printf("SEND ha resetituito <= 0\n");
            exit(-1);
        }

        ret = send(sd,buff,strlen(buff)+1,0);
        if (ret <= 0){
            printf("SEND ha resetituito <= 0\n");
            exit(-1);
        }
    }

}

int RUBRICAcontrol(char utente[]){
    char usr_raw[25];   //per verificare se utente è in rubrica
    FILE* fd;

    sprintf(buff,"RUBRICA/Rubrica-%s.txt",d.user);   //meccanismo per aprire rubrica utente
    fd = fopen(buff,"r");


    while(fgets(buff,200,fd)){      //verifico se utente è in rubrica
        sscanf(buff,"%s",usr_raw);
        if (!strcmp(utente,usr_raw)){     
            break;
        }
    }
    fclose(fd);

    if (strcmp(utente,usr_raw)){        // utente non in rubrica
        return 0;
    }

    return 1;  // utente presente in rubrica
}

void CRONOLOGIA(char utente[]){
    FILE* chat;
    char asterisco[25];
    char user_raw[25];  //serve per fare confronto con USER
    char USER[25];  // serve per il formato messaggi chat [utente]
    // nome chat si basa su ordine alfabetico
    if(strcmp(d.user,utente) < 0)
        sprintf(buff,"GENERAL/%s+%s-chat.txt",d.user,utente);
    else
        sprintf(buff,"GENERAL/%s+%s-chat.txt",utente,d.user);
    chat = fopen(buff,"r+");

    if(chat == NULL)      // caso in cui non ci sono messaggi precedenti in chat
        return;

    // costruisco formato [utente] in USER -> non posso modificare utente
    sprintf(USER,"[%s]",utente);

    printf("\nCHAT:\n\n");
    // esamina la prima parola di ogni riga (*->messaggio letto per la prima volta) -> togliere *
    // se la prima parola non è un asterisco è un messaggio già letto
    while( fscanf(chat,"%s",asterisco) != EOF ){
        if (!strcmp(asterisco,"*")){    // se trovi un carattere *
            fseek(chat,strlen(asterisco),SEEK_CUR);
            fscanf(chat,"%s",user_raw);
            fseek(chat,-(strlen(user_raw)+2), SEEK_CUR);
            if (!strcmp(user_raw,USER)){ // se username di quella riga è [utente] (contenuto in)  
                fprintf(chat," ");  //togli l'asterisco ho visto messaggio pendente
                fgets(buff,200,chat);
                printf("*%s",buff); //stampa comunque l'asterisco per fare capire all utente fosse messaggio pedente
            }
            else{
                fgets(buff,200,chat);   //il messagggio con l'asterisco era il mio lo stampo normalemnte
                printf("%s",buff);      // l'asterisco lo lascio perchè il messaggio non è stato ancora visto da altro utente
            }
        }// non hai trovato asterisco
        else{
            fseek(chat,-strlen(asterisco),SEEK_CUR);
            fgets(buff,200,chat);   //messaggio normale (senza asterischi) -> stampa di default
            printf("%s",buff);
        }

    }

    fclose(chat);
    return;
 
}

void CHAT(char utente[]){
    struct chat* p,*q; //per gestire lista delle chat globali

    int  new;   //socket necessario in caso utente mi scrive per la prima volta quando sono dentro una chat con altro utente
    struct sockaddr_in deviceClient;   // struttura per bind con new socket

    struct chat* registro,* index; // servono nel caso di chat broadcast (locale)
    char new_utente[25];   //variabile di appggio per aggiungere nuovi partecipanti alla chat
    char file_name[25];
    char add[10]; // per verificare \\a\0 e share

    fd_set masterChat, workChat;
    int fdmax,i,file_byte;
    int socket;

    FD_ZERO(&masterChat);
    FD_ZERO(&workChat);

    FD_SET(STDIN_FILENO,&masterChat);
    fdmax = STDIN_FILENO;
    // se il server è on lo aggiungo ai socket da monitorare
    if (server_status){
        FD_SET(sd,&masterChat);
        if (sd > fdmax)
            fdmax = sd;
    }   

    // controllo se esiste già connessione con utente e nel caso aggiungo il socket a quelli da monitorare
    p = head;               
    while (p != NULL ){
        if(!strcmp(p->user,utente)){
            FD_SET(p->socket,&masterChat);
            if (p->socket > fdmax)
                fdmax = p->socket;
            break;
        }
        p = p->punt;
    }

    // tengo nota degli utenti con cui chatto dentro questa chat (di default quello con cui inizio la chat)
    registro = (struct chat*)malloc(sizeof(struct chat));
    strcpy(registro->user,utente);
    registro->punt = NULL;

    printf("\nCHAT con %s:\n\n",utente);

    while('\n'!= getchar()); //pulizia residua di scanf

//    ### CICLO ALL INTERNO DELLA CHAT ### fino a lettura '\q'
    while(1){

        workChat = masterChat;
        select(fdmax+1, &workChat, NULL, NULL, NULL);
        for ( i = 0 ; i <= fdmax ; i++ ){
            if(FD_ISSET(i,&workChat)){

                if( i == STDIN_FILENO ){
                    fgets(buff,200,stdin);
                
                    if (!strcmp(buff,"\\q\n")){ // --> si esce dalla chat ma si conservano connessioni tcp nella lista di struct chat
                        printf("chiusura chat in corso\n");
                        return;
                    }
                    if (!strcmp(buff,"\\u\n")){ // --> aggiungere partecipanti alla chat
                        //controllo se server è online
                        if(!server_status){
                            printf("impossibile aggiungere partecipanti server OFFLINE.\n");
                            continue;
                        }
                        //mostra lista
                        showLIST();
                        printf("Digita '\\a' + uno dei nomi sopra elencati per aggiungerlo alla chat\n");

                        //aggiungi partecipante -> tutti i socket in masterChat
                        strcpy(new_utente,"invalid"); //controllo sul secondo parametro                       

                        fgets(buff,200,stdin);
                        sscanf(buff,"%s %s",add,new_utente);

                        // --- CONTROLLO VALIDITA' nuovo utente ---
                        if(strcmp(add,"\\a") || !strcmp(new_utente,"invalid")){
                            printf("INPUT non valido - continua a Chattare...\n");
                            continue;
                        }
                        if (!RUBRICAcontrol(new_utente)){
                            printf("Utente richiesto %s non presente in Rubrica\ncontinua a Chattare...\n",new_utente);
                            continue;
                        }

                        index = registro;
                        while(index != NULL){
                            if(!strcmp(index->user,new_utente)){
                                printf("utente già presente in chat.\n");
                                break;
                            }
                            index = index->punt;
                        }
                        if (index)  //utente già presente in chat
                            continue;

                        // --- FINE CONTROLLO ---

                        printf("l'utente (%s) è stato unito alla chat.\nI messaggi inviati arriveranno anche a lui\n\n",new_utente);
                        // inserisco nuovo utente nel registro di questa chat
                        index = (struct chat*)malloc(sizeof(struct chat));  
                        strcpy(index->user,new_utente);
                        index->punt = registro;
                        registro = index;

                        continue;
                    }
                    // SHARE FILE --------------------------------- 
                    sscanf(buff,"%s %s",add,file_name);
                    if (!strcmp(add,"share")){
                        if(!checkfile(file_name,&file_byte)){ //al ritorno dalla funzione è inizializzato file_byte
                            printf("\nFile non trovato. Controllare directory e riprovare.\n");
                            continue;
                        }
                        // poichè il file-sharing non deve passare dal server mi devo assicurare che
                        // gli utenti presenti in questa chat (contenuti in registro) siano online e
                        // abbiano già una connessione attiva, e quindi si devono trovare nella lista puntata da head
                        index = registro;
                        while(index != NULL){
                            p = head;
                            while(p){
                                if(!strcmp(p->user,index->user))
                                    break;
                                p = p->punt;
                            }
                            if (!p){
                                printf("\nImpossibile mandare file a %s senza passare dal server.\n",index->user);
                                index = index->punt;
                                continue;
                            }
                            // se arriviamo qui p->user è un utente che può ricevere il file
                            SHAREfile(file_name,file_byte,p->socket);
                            index = index->punt;
                        }
                        continue;
                    }
                    // --------------------------------------------
                    printf("\n");
                    // se arriviamo qui allora avevamo digitato un messaggio da inviare
                                       
                    index = registro;
                    while(index != NULL){
                        socket = INVIO(index->user);                  
                        if (socket > 0){
                            // controllo se è nella lista &masterChat se non c è lo aggiungo 
                            if (!FD_ISSET(socket,&masterChat)){
                                FD_SET(socket,&masterChat);
                                if ( socket > fdmax )
                                    fdmax = socket;
                            }
                        }
                        if (socket == -1){
                            printf("Il server è offline impossibile recapitare il messaggio all' utente [%s].\n",index->user);
                            printf("E' possibile continuare a messaggiare nelle chat già aperte con utenti online\n");
                            return;
                        }
                        index = index->punt;
                    }
                    
                }
                else{  
                    
                    // ----------------------------------------------------------
                    if (i == sd){             // ### IL SERVER MI HA INVIATO UN MESSAGGIO
                        /*QUALCUNO STA CERCANDO DI INIZIARE
                            UNA CHAT CON ME - OPPURE MI ARRIVA UN ACK*/
                        
                        // prendo messaggio server (ack o username)
                        ret = recv(sd,&len,sizeof(len),0);
                        if (ret <= 0){
                            printf("SERVER DISCONNESSO\n");     
                            server_status = 0;      //server offline
                            close(sd);
                            FD_CLR(sd,&master);
                            FD_CLR(sd,&masterChat);
                            continue;
                        }
                        dim = ntohs(len);
                        if (dim == 1000){ //caso in cui server mi informa nuovo device si sia loggato con mie credenziali
                            printf("Sessione scaduta, nuovo login effettuato da altro device\n");
                            close(sd);
                            // dealloco tutto prima di uscire (non serve salvare time_stamp perchè tanto c'è altro device loggato)
                            p = head;
                            while (p != NULL ){
                                head = p->punt;
                                close(p->socket);
                                free(p);
                                p = head; 
                            }
                            exit(1);
                        }
                        // ACK QUALCUNO HA LETTO MIEI MESSAGGI PENDENTI
                        if (dim == 1001){
                            ret = recv(sd,&len,sizeof(len),0);
                            if (ret < 0){
                                printf("problemi con reccv per ack notifica\n");
                            }

                            dim = ntohs(len);

                            ret = recv(sd,utente,dim,0);      // in utente è contenuto l'username che ha letto miei messaggi
                            if (ret <= 0){
                                printf("recv di SD ha dato <= 0\n");
                            }

                            printf("NOTIFICA - {%s} ha letto i tuoi messaggi.\n",utente);
                            continue;
                        }
                        // ### se arriviamo qui il server mi sta inviando messaggio iniziale di un nuovo utente ###

                        ret = recv(sd,new_utente,dim,0);      // in new_utente è contenuto l'username del device che ha iniziato chat
                        if (ret <= 0){
                            printf("recv di SD ha dato <= 0\n");
                        }

                        ret = recv(sd,&len,sizeof(len),0);
                        if (ret <= 0){
                            printf("recv di SD ha dato <= 0\n");
                        }

                        dim = ntohs(len);

                        ret = recv(sd,buff,dim,0);      // in buff è contenuto il messaggio dal device che ha iniziato chat
                        if (ret <= 0){
                            printf("recv di SD ha dato <= 0\n");
                        }
                
                        // creo nuovo socket per connessione con mittente, la connect la fa lui
                        dim = sizeof(deviceClient);

                        new = accept(p2p,(struct sockaddr*)&deviceClient,&dim);
                        // si pianta sulla accept se si effettua la disconnessione e ricollegamento sulla stessa porta
                        if(new <= 0){
                            perror("ERRORE: ");
                            printf("l' utente  %s ha provato a mandarti un messaggio iniziale\n, ma errore di accept.\n",new_utente);
                            continue;
                        }

                        // creo struct nuova chat 
                        p = (struct chat*)malloc(sizeof(struct chat));
                        strcpy(p->user,new_utente);
                        p->socket = new;
                        p->punt = NULL;
                        // inserimento in testa alla lista delle chat
                        if(head)
                            p->punt = head;
                        head = p;
                        // mostro il messaggio ATTENZIONE
                        // potrebbe essere il caso che io sia già dentro alla chat con chi mi ha inviato
                        // il messaggio in questo caso devo aggiungere il socket a quelli da monitorare
                        index = registro;
                        while( index){
                            if (!strcmp(index->user,p->user)){
                                if (!FD_ISSET(p->socket,&masterChat)){ //questo if è di sicurezza, ma dovrebbe essere sempre vero
                                    FD_SET(p->socket,&masterChat);
                                    if ( p->socket > fdmax )
                                        fdmax = p->socket;
                                }

                                printf("[%s] : %s\n",p->user,buff);   
                                break;
                            }
                            index = index->punt;
                        }
                        // utente che mi ha scritto esterno alla chat
                        if(!index){
                            printf("#### NOTIFICA - {%s} ha iniziato una chat con te.\nMessaggio:\n%s\n",p->user,buff);
                            printf("Esci dall'attuale chat o aggiungilo con \\u in questa chat creando un gruppo\n");
                        }
                       
                       // non lo aggiungo ai socket da monitorare dentro select della chat, mostro solo la notifica
                       // il nuovo socket nella select generale ci entra quando uscirò dalla chat

                    } // --------------------------------------
                    else{
                        
                         // MESSAGGIO DA UTENTE/I ALL'INTERNO DELLA CHAT
                        p = head;
                        while ( p != NULL ){    // scopro da chi viene il messaggio

                            if (p->socket == i )
                                break;
                            q = p;
                            p = p->punt;
                        }
                        //prelevo il messaggio in buff
                        ret = recv(i,&len,sizeof(len),0);
                        if (ret <= 0){
                            printf("Utente %s disconnesso.\n",p->user);
                            if (head == p)
                                head = p->punt;
                            else
                                q->punt = p->punt;
                            free(p);
                            close(i);
                            FD_CLR(i,&masterChat);
                            if ( FD_ISSET(i,&master))  // se non sono mai uscito da chat il socket p->socket non è mai stato aggiunto a master
                                FD_CLR(i,&master);      // ma solo in masterChat
                            continue;
                        }
                        dim = ntohs(len);
                        ret = recv(i,buff,dim,0);      // in buff è contenuto il messaggio dal device con cui sono in chat
                        if (ret <= 0){
                            printf("Utente %s disconnesso. ERRORE IRRISOLVIBILE\n",p->user);
                            exit(-1);
                        }

                        //FILE SHARE ----------------------------------------
                        if (!strcmp(buff,"share")){
                            printf("L' utente [%s] ti sta inviando un file\n",p->user);
                            RECEIVEfile(i);
                            continue;
                        }

                        // -------------------------------------------------
                        
                        printf("[%s] : %s\n",p->user,buff);

                    }
                }
            }

        }
    }
}

void showLIST(){

    d.req = 30;

    //invio informazioni di routine
    sprintf(buff,"%s %s %d %d",d.user,d.passwd,d.port,d.req);
    dim = strlen(buff)+1;
    len = htons(dim);
    ret = send(sd,&len,sizeof(len),0);
    if (ret <= 0){
        server_status = 0;
        return;
    }
    
    ret = send(sd,buff,strlen(buff)+1,0);
    if (ret <= 0){
        printf("SEND ha resetituito <= 0\n");
        exit(-1);
    }
    
    // ricevo risposta
    ret = recv(sd,&len,sizeof(len),0);
    if(ret <= 0){
        printf("problemi con recv \\u\n");
        exit(1);
    }
    dim = ntohs(len);
    ret = recv(sd,buff,dim,0);
    if(ret <= 0){
        printf("problemi con recv \\u\n");
        exit(1);
    }
    
    //stampo a video
    printf("%s\n\n",buff);
}

int INVIO(char utente[]){
    int i = 0;
    int new_socket;
    char msg[175];
    struct sockaddr_in user_device;
    struct chat* p = head;
    FILE* chat;
    
    strcpy(msg,buff);       // passo il messaggio da buff a msg

    if (strcmp(d.user,utente) < 0 ) //controllo alfabetico per determinare nome file chat
        sprintf(buff,"GENERAL/%s+%s-chat.txt",d.user,utente);
    else
        sprintf(buff,"GENERAL/%s+%s-chat.txt",utente,d.user);
    chat = fopen(buff,"a"); //apro in append
    if (chat == NULL)
        printf("errore apertura chat\n");
    
    strcpy(buff,msg); //necessario perchè in caso di chat di gruppo alla prossima chiamata
//                      di INVIO() nel buff non ci sarbebe più il messaggio originale


    while (p != NULL ){         // controllo se la connessione con quell utente c'è già
        if (!strcmp(p->user,utente)){
            // invio il messaggio in msg al socket p->socket
            dim = strlen(msg)+1;
            len = htons(dim);
            ret = send(p->socket,&len,sizeof(len),0);
            if (ret <= 0){
                printf("SEND ha resetituito <= 0\n");
                exit(-1);
            }
            ret = send(p->socket,msg,strlen(msg)+1,0);
            if (ret <= 0){
                printf("SEND ha resetituito <= 0\n");
                exit(-1);
            }
            //scrittura in chat senza asterisco
            fprintf(chat,"  [%s] : %s",d.user,msg);
            fclose(chat);
            //
            return(p->socket);
        }
        p = p->punt;
    }

    // ### non ho nessuna connessione attiva con l'utente -> devo chiedere al server

    if (!server_status){    //se il server è offline non posso mandare messaggio
        printf("\nil SERVER è offline impossibile recapitare il messaggio\n");
        return -1;
    }
    
    d.req = 3;

//     1 - invio informazioni di routine
    sprintf(buff,"%s %s %d %d",d.user,d.passwd,d.port,d.req);
    dim = strlen(buff)+1;
    len = htons(dim);

    ret = send(sd,&len,sizeof(len),0);
    if (ret <= 0){
        server_status = 0;
        return -1;
    }

    ret = send(sd,buff,strlen(buff)+1,0);
    if (ret <= 0){
        printf("SEND ha resetituito <= 0\n");
        exit(-1);
    }

//       2 - invio username dell utente(utente) con cui vorrei chattare e messaggio(msg)

    sprintf(buff,"%s",utente);
    
    while(i < 2){

        dim = strlen(buff)+1;
        len = htons(dim);

        ret = send(sd,&len,sizeof(len),0);
        if (ret <= 0){
            printf("SEND ha resetituito <= 0\n");
            exit(-1);
        }

        ret = send(sd,buff,strlen(buff)+1,0);
        if (ret <= 0){
            printf("SEND ha resetituito <= 0\n");
            exit(-1);
        }

        i++;
        if (i == 1){
            sprintf(buff,"%s",msg);   
        }

    }

//   3 - prendo risposta: ack(port)

    ret = recv(sd,&len,sizeof(uint16_t),0);
    if (ret <= 0){
        printf("RECV ha resetituito <= 0\n");
        exit(-1);
    }
    dim = ntohs(len);       //dim == porta 

    if (dim){   // se la porta è diversa da 0 utente è online -> creo socket comunicazione e inserisco in lista chat 

        // scrivo in CHAT senza asterisco
        fprintf(chat,"  [%s] : %s",d.user,msg);
        fclose(chat);
        
        new_socket = socket(AF_INET,SOCK_STREAM,0);

        memset(&user_device,0,sizeof(user_device));
        user_device.sin_family = AF_INET;
        user_device.sin_port = htons(dim+100);
        inet_pton(AF_INET, "localhost",&user_device.sin_addr);


        ret = connect(new_socket,(struct sockaddr*)&user_device,sizeof(user_device));       // funziona se il socketP2P(server-device) dell altro utente è on
        if (ret == -1 ){
            printf("\nIMPOSSIBILE STABILIRE CONNESSIONE con - %s -,\n riprovare in un secondo momento",utente);
            close(new_socket);
            return 0;
        }

        p = (struct chat*)malloc(sizeof(struct chat));
        strcpy(p->user,utente);
        p->socket = new_socket;

        p->punt = NULL;     //inserimento in lista
        if (head)
            p->punt = head;
        head = p;

        return new_socket;
    }
    // il server non è riuscito a contattare device destinatario
    printf("%s e' OFFLINE\n\n", utente);
    //scrivo in CHAT con asterisco
    fprintf(chat,"* [%s] : %s",d.user,msg);
    fclose(chat);

    return 0; 
    //questa funziona ritorna: un socket già esistente se la comunicazione era già stata creata
    //                         un nuovo socket se il server mi dice che ora il mittente è diventato online
    //                         0 se il server mi dice che l'utente a cui voglio scrivere è offline    ##DEBUG -> se è offline lo devo togliere dal set globale? oppure 
    //                         -1 se devo mandare un messaggio a utente non ancora collegato e server è offline
}

void RUBRICA(){
    FILE* fd;
    char usr_raw[25];

    sprintf(buff,"RUBRICA/Rubrica-%s.txt",d.user);   //meccanismo per aprire rubrica utente
    fd = fopen(buff,"r");

    printf("\nRUBRICA di %s:\n\n",d.user);

    while(fgets(buff,200,fd)){      //stampo tutti i nomi in rubrica
        sscanf(buff,"%s",usr_raw);
        printf("%s\n",usr_raw);
    }
    fclose(fd);
    printf("\n");
}

void OUT(){
    FILE* fd;
    struct chat* p;
    struct tm* timeinfo;
    time_t rawtime;

    sprintf(buff,"GENERAL/log-%s.txt",d.user);   //meccanismo per aprire/creare log utente
    fd = fopen(buff,"w"); //apro in scrittura - nuova informazione
    if (fd == NULL){
        printf("errore apertura file\n");
        exit(-1);
    }

    // verifico se server è online o offline

    if (server_status){     // ONLINE
        //lo informo che mi disconnetto
        printf("---DISCONNESSIONE DAL SERVER\n");

        d.req = 6;

        fprintf(fd,"NULL\n");     // significa che al prossimo avvio non dovrò mandare ack a server con logout

        sprintf(buff,"%s %s %d %d",d.user,d.passwd,d.port,d.req);
        dim = strlen(buff)+1;
        len = htons(dim);

        ret = send(sd,&len,sizeof(len),0);
        if (ret <= 0){
            printf("SEND ha resetituito <= 0\n");
            exit(-1);
        }

        ret = send(sd,buff,strlen(buff)+1,0);
        if (ret <= 0){
            printf("SEND ha resetituito <= 0\n");
            exit(-1);
        }
        // non mi serve risposta

    }
    else{       // OFFLINE
        printf("---SALVATAGGIO INFORMAZIONI - non spegnere device\n");
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        fprintf(fd,"%d:%d:%d\n",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);

    }
    
    fclose(fd);

    printf("---CHIUSURA IN CORSO\n");

    // dealloco tutta la lista di CHAT puntata da head 
    p = head;
    while (p != NULL ){
        head = p->punt;
        close(p->socket);
        free(p);
        p = head; 
    }

    close(sd);
    close(p2p);

    printf("---CHIUSURA EFFETTUATA\n");

    // chiudo tutti i socket


    sleep(1);
    printf("-\nArrivederci!\n\n");
    exit(1);
}

int checkfile(char file_name[],int *num_bytes){
    FILE* file;
    file = fopen(file_name,"r");
    if (!file){
        return 0;
    }
    fseek(file,0,SEEK_END);
    *num_bytes = ftell(file);
    fclose(file);
    return 1;

}

void SHAREfile(char file_name[], int num_bytes, int socket){
    FILE* file;
    int read_bytes;
    u_int32_t dimensione;
    strcpy(buff,"share");

    file = fopen(file_name,"r");
    if (file <= 0){
        printf("errore apertura file");
        return;
    }

    dim = strlen(buff)+1;
    len = htons(dim);
    printf("\ninvio keywords %s\n",buff);
    // invio keywords
    ret = send(socket,&len,sizeof(len),0);
    if (ret <= 0 ){
        printf("errore");
        return;
    }
    ret = send(socket,buff,dim,0);
    if (ret <= 0 ){
        printf("errore");
        return;
    }
    // invio nome file
    printf("\ninvio nome file %s\n",file_name);
    dim = strlen(file_name)+1;
    len = htons(dim);
    ret = send(socket,&len,sizeof(len),0);
    if (ret <= 0 ){
        printf("errore");
        return;
    }
    ret = send(socket,file_name,dim,0);
    if (ret <= 0 ){
        printf("errore");
        return;
    }
    // invio numero totale bytes
    printf("\ninvio dimensione file %d\n", num_bytes);
    dimensione = htonl(num_bytes);
    ret = send(socket,&dimensione,sizeof(dimensione),0);
    if (ret <= 0 ){
        printf("errore");
        return;
    }
    // ciclo per inivare il file
    while( !feof(file) ){
        read_bytes=fread(vett,sizeof(uint8_t),sizeof(vett),file);
        len = htons(read_bytes);
        ret = send(socket,&len,sizeof(len),0);
        if (ret <= 0 ){
            printf("errore");
            return;
        }
        ret = send(socket,vett,read_bytes,0);
        if (ret <= 0 ){
            printf("errore");
            return;
        }
    }
    printf("\nFILE INVIATO!\n\n");

    fclose(file);
    
} 

void RECEIVEfile(int socket){
    FILE* file;
    int file_byte;
    int rcv_byte;
    uint32_t dimensione;
    char file_name[25];
    // ricevo file_name
    ret = recv(socket,&len,sizeof(len),0);
    if(ret <= 0){
        printf("ERRORE");
        return;
    }
    dim = ntohs(len);
    ret = recv(socket,file_name,dim,0);
    if(ret <= 0 ){
        printf("ERRORE");
        return;;
    }
    printf("\nricevuto file name %s", file_name);
    // Di default il file viene salvato in /GENERAL/nomeutente/nomefile
    // ma se la directory non è ancora stata creata lo salva nella directory GENERAL
    // aggiungendo al nome del file il nome dell utente
    sprintf(buff,"GENERAL/%s/%s",d.user,file_name);
    file = fopen(buff,"w");
    if (file <= 0){
        printf("\nNon presente directory specifica, il file verrà salvato nella directory GENERAL\n");
        sprintf(buff,"GENERAL/%s--%s",d.user,file_name);
        file = fopen(buff,"w");
        if (file <= 0){
            printf("ERRORE APERTURA FILE\n");
            exit(-1);
        }
    }
    // ricevo numero byte
    ret = recv(socket,&dimensione,sizeof(dimensione),0);
    if(ret <= 0){
        printf("ERRORE");
        return;
    }
    file_byte = ntohl(dimensione);
    printf("\ndimensione FILE %d\n", file_byte);
    while(file_byte > 0){
      //  printf("File BYte rimasti: %d\n",file_byte);
        ret = recv(socket,&len,sizeof(len),0);
        if(ret <= 0){
            printf("ERRORE");
            return;
        }
        dim = ntohs(len);
        rcv_byte = recv(socket,vett,dim,0);
        if(ret <= 0 ){
            printf("ERRORE");
            return;;
        }
        fwrite(vett,1,rcv_byte,file);
        file_byte -= rcv_byte;
        // printf("Byte letti: %d\n\n",rcv_byte);
        

    }

    fclose(file);
    printf("File ricevuto correttamente\n\n");

}