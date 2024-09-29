#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define tm_len 9

char buff[1024];
uint16_t len;   // per lunghezza messaggi invio/ricez
int ret,dim;  //verificare esito syscall e appoggio
fd_set master,work; //select
struct tm* timeinfo;
time_t rawtime;
struct device{
    char user[25];
    char passwd[25];
    int port;
    int req;    //1 SIGNUP 2LOGIN
    int islog;  //1 time_stamp si riferisce al login 0 al log out.
    int socket_tcp; //per gestire disconnessione improvvisa
    char time_stamp[tm_len];
    struct device* punt;
};
struct device* utenti;
struct device* p;

void HELP();
void LIST();
void ESC();
void LOGIN(int);    // req 2
void SIGNUP(int);   // req 1
void NEWCHAT(int);  // req 3
void sendLIST(int);     // req 30
void sendLOG(int);      // req 10
void sendNOTIFICA(int); //req 20
void OFFDEVICE();    // req 6
void menu();
void logRefresh(char[]);

int main(int argc, char** argv){
    

    int fdmax;
    int i = 0;
    
    int listener, new;    //  socket
    struct sockaddr_in server, client;

    int port = 4242;
    if ( argc > 1 )
        sscanf(argv[1],"%d",&port);


    FD_ZERO(&master);
    FD_ZERO(&work);

    

    listener = socket(AF_INET, SOCK_STREAM,0);

    memset(&server,0,sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET,"localhost",&server.sin_addr);

    ret = bind(listener,(struct sockaddr*)&server, sizeof(server));   
    if ( ret < 0 ){
        printf("PROBLEMI CON LA BIND\n");
        exit(-1);
    }

    ret = listen(listener,10);

    FD_SET(listener,&master);
    FD_SET(STDIN_FILENO,&master);
    fdmax = listener;

    for(i = 0; i < 30; i++){
        printf("*");
    }
    printf("\nSERVER STARTED SUCCESSFULLY\n");
    for(i = 0; i < 30; i++){
        printf("-");
    }

    menu();

    while(1){

        work = master;
        select(fdmax+1, &work, NULL, NULL, NULL);
        for ( i = 0 ; i <= fdmax ; i++ ){

            if( FD_ISSET(i,&work) ){
//      ######################## GESTIONE MENU SERVER #################################################
                if (i == STDIN_FILENO){
                    int opt;
                    scanf("%d", &opt);

                    switch(opt){

                        case 1:
                            HELP();
                            break;
                        case 2:
                            LIST();
                            break;
                        case 3:
                            ESC();
                            break;
                        default:
                            printf("DIGITA INPUT VALIDO\n");
                            break;
                    }
                }
//      ########################## GESTIONE RICHIESTE DEVICE #########################################
                else{   

                    if (i == listener){             // ### ACCETTO NUOVA CONNESSIONE
                        dim = sizeof(client);
                        printf("ACCETTATA NUOVA CONNESSIONE\n");
                        new = accept(listener, (struct sockaddr*)&client, &dim);
                        FD_SET (new, &master);
                        if ( new > fdmax )
                            fdmax = new;
                    }
                    else{                      // ### GESTISCO RICHIESTA SINGOLI DEVICE
                        p = (struct device*) malloc(sizeof(struct device));
                        printf("E' arrivata una richiesta da gestire\n");
                        ret = recv(i,&len,sizeof(len),0);
                        if (ret <= 0){          // ### DEVICE DISCONNESSO OPPURE ERRORE  
                            FD_CLR(i,&master);
                            close(i); 
                            if (ret < 0){
                                printf("Errore dovuto alla recv\n");
                                exit(-1);
                            }
                            // ### GESTIRE DISCONNESSIONE DEVICE IMPROVVISA (senza passare da funzione)
                            printf("CHIUSA CONNESSIONE IMPROVVISA\n");
                            free(p);
                            p = utenti;
                            while (p != NULL){
                                if (p->socket_tcp == i){
                                    p->islog = 0;   //device non loggato
                                    time(&rawtime);
                                    timeinfo = localtime(&rawtime);
                                    sprintf(p->time_stamp,"%d:%d:%d",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);      //inserisco time_stamp logout
                                }
                                p = p->punt;
                            }
                            
                            continue;
                        }
                        // ### Normale amminastrazione, prelevo i dati e eseguo
                        dim = ntohs(len);
                        ret = recv(i,buff,dim,0);
                        if (ret < 0){
                            printf("Errore dovuto alla recv\n");
                            exit(-1);
                        }
                        sscanf(buff,"%s %s %d %d",p->user,p->passwd,&p->port,&p->req);      // indirizzo i valori sensibili attraverso p
                        printf("req di tipo %d\n",p->req);
                        
                        if (p->req > 2){ // utente già loggato, prendo la sua struttura dati
                            struct device* q = p;
                            p = utenti;
                            while(p != NULL ){
                                if (!(strcmp(p->user,q->user))){
                                    p->req = q->req;
                                    free(q);    // libero struct device creata per identificare mittente
                                    break;
                                }
                                p = p->punt;
                            }
                        }
                        switch(p->req){
                            case 1:
                                SIGNUP(i);
                                break;
                            case 2:
                                LOGIN(i);
                                break;
                            case 3:
                                NEWCHAT(i);
                                break;
                            case 6:
                                OFFDEVICE();
                                break;
                            case 10:
                                sendLOG(i);
                                break;
                            case 20:
                                sendNOTIFICA(i);
                                break;
                            case 30:
                                sendLIST(i);
                                break;
                            default:
                                printf("RICHIESTA NON VALIDA\n");
                                break;
                        }
                        

                    }
                    
                }
            }
        }     

    }

}


void HELP(){
    int i;
    printf("\n");
    for (i = 0; i < 30; i++)
        printf("#");
    printf("\nQuesto processo Server gestisce una piccola applicazione di messaggistica:\n");
    printf("Con l'opzione 2 (LIST) viene mostrata a scherma una lista degli utenti attualmente online nel servizio, con relativo istante di Login e porta di ascolto\n");
    printf("Con l'opzione 2 (ESC) il processo server subirà uno shutdown, in questo caso nessun utente potrà loggarsi nell applicazione ma quelli già online potranno continuare a chattare\n");
    printf("\n");
    for (i = 0; i < 30; i++)
        printf("#");
    printf("\n");
    menu();
    
}

void LIST(){
    int cont = 0;
    printf("\nLista degli utenti ONLINE:\n");
    p = utenti;
    while(p != NULL){
        if (p->islog){
            printf("<%s*%s*%d>\n",p->user,p->time_stamp,p->port);
            cont++;
        }

        p = p->punt;
    }
    printf("Totale Utenti: %d.\n\n",cont);

}

void sendLIST(int sd){
    struct device* q;   //var di appoggio, p punta a struct device chiamante
    strcpy(buff,"Lista utenti online:\n");
    q = utenti;
    // creo un buff fomato da utenti online alternati da \n
    while(q != NULL){
        if (q->islog && strcmp(p->user,q->user)){
            strcat(buff,q->user);
            buff[strlen(buff)+1] = '\0';
            buff[strlen(buff)] = '\n';       
        }
        q = q->punt;
    }

    //invio buff contenente lista utenti attivi

    dim = strlen(buff)+1;
    len = htons(dim);
    ret = send(sd,&len,sizeof(len),0);
    if (ret <= 0){
        printf("Errore send <=0\n");
        exit(-1);
    }

    ret = send(sd,buff,strlen(buff)+1,0);
    if (ret <= 0){
        printf("Errore send <=0\n");
        exit(-1);
    }
}

void ESC(){
    struct device* q;
    printf("\nDISCONNESSIONE EFFETTUATA\n");
    p = utenti;
    while (p != NULL){
        q = p;
        p = p->punt;
        free(q);
    }
    exit(1);
}

void SIGNUP(int sd){

    char usr_raw[20];
    FILE* fd;
    fd = fopen("./GENERAL/utenti.txt","r+"); //apro in lettura per fare controllo
    if (fd == NULL){
        printf("errore apertura file\n");
        exit(-1);
    }

    while(fgets(buff,200,fd)){
        sscanf(buff,"%s",usr_raw);
        if (!strcmp(p->user,usr_raw) ){     //Esiste già uno user con quel nome 
            fclose(fd);
            len = htons(0);                 //restituire 0 (SIGNUP FALLITO)
            ret = send(sd,&len,sizeof(len),0);
            if (ret < 0){
                printf("ERRORE send dentro SIGNUP\n");
                free(p);
                return;
            }
            return;
        }
    }
    // sono arrivato alla fine del file allora signup ha avuto successo


    fprintf(fd,"%s %s\n",p->user,p->passwd);  // inserisco nuovo user

    fclose(fd);     
    len = htons(1);                      // restituisco 1
    ret = send(sd,&len,sizeof(len),0);
    if (ret < 0){
        printf("ERRORE send dentro SIGNUP\n");
    }

    free(p);    //distruggo struttura device che non serve più
}

void LOGIN(int sd){   //Invia al mittente 0 se login fallita 1 altrimenti - !!!!devo controllare porte non duplicate DEBUG
    struct tm* timeinfo;
    time_t rawtime;
    struct device* q,*r;
    char usr_raw[20],psw_raw[20];
    FILE* fd;
    fd = fopen("./GENERAL/utenti.txt","r");     // apertura in sola lettura
    if (fd == NULL){
        printf("errore apertura file\n");
        exit(-1);
    }

    while(fgets(buff,200,fd)){
    
        sscanf(buff,"%s %s",usr_raw,psw_raw);
        if (!strcmp(p->user,usr_raw) && !strcmp(p->passwd,psw_raw)){     //Login Avvenuta con Successo
            fclose(fd);
            len = htons(1);
            ret = send(sd,&len,sizeof(len),0);
            if (ret < 0){
                printf("ERRORE send dentro LOGIN\n");
                free(p);
                return;
            }
            p->socket_tcp = sd;
            p->islog = 1;   //device loggato
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            sprintf(p->time_stamp,"%d:%d:%d",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);      //inserisco time_stamp

            q = utenti;             // controllo se device si era già loggato in precedenza e nel caso elimno la precedente sessione non più significativa
            while (q != NULL){
                if (!strcmp(p->user,q->user)){
                    if(q->islog){               // caso doppio login -> chiudo connessione con precedente login
                        //devo gestire caso disconnessione forzata o se per server offline - DEBUG-risolto
                        // chiudo socket vecchia login ma lui non sa se è perche il server è andato OFF o se altro utente connesson con sue credenziali
                        // invio 1000 per nuovo login e nulla per server offline
                        len = htons(1000);
                        ret = send(q->socket_tcp,&len,sizeof(len),0);
                        close(q->socket_tcp);
                        FD_CLR(q->socket_tcp,&master);
                    }                     
                    if (q == utenti)
                        utenti = q->punt;
                    else{
                        r->punt = q->punt;
                    }
                    free(q);
                    break;
                }
                r = q;
                q = q->punt;
            }

            p->punt = NULL;         // inserimento in testa
            if (utenti){
                p->punt = utenti;
            }
            utenti = p;

            return;
        }
    }
    free(p);    // Login Fallita
    fclose(fd);
    len = htons(0);
    ret = send(sd,&len,sizeof(len),0);
    if (ret < 0){
        printf("ERRORE send dentro LOGIN\n");
    }
    return;
}

void sendLOG(int sd){
    FILE* log;
    char file_name[50];
    char riga[50];
    char delimiter[25];
    int conto = 0;

    strcpy(buff,"Notifiche:\n\n");

    sprintf(file_name,"GENERAL/log-%s.txt",p->user);
    log = fopen(file_name,"r"); //apro in lettura per leggere eventuali righe di notifica

    if (log != NULL){   //se il file esiste
        while( fscanf(log,"%s",delimiter) != EOF ){
            if (!strcmp(delimiter,"<")){    // se trovi un carattere <
                fseek(log,-strlen(delimiter),SEEK_CUR);
                fgets(riga,50,log);
                strcat(buff,riga);
                conto++;
            }
        }
        fclose(log);
    }

    
    

    if (conto == 0){
        strcpy(buff,"Non sono arrivati messaggi mentre eri assente.\n");
    }

    // azzero file di log (eventuale informazione di logout non più significativa + eventuali notifiche)
    log = fopen(file_name,"w"); 
    fclose(log);

    //invio buff a p->user
    dim = strlen(buff)+1;
    len = htons(dim);
    ret = send(sd,&len,sizeof(len),0);
    if (ret <= 0){
        printf("Errore send <=0\n");
        exit(-1);
    }

    ret = send(sd,buff,strlen(buff)+1,0);
    if (ret <= 0){
        printf("Errore send <=0\n");
        exit(-1);
    }

}

void NEWCHAT(int sd){

    char user_raw[25];  // viene salvato l'utente con cui p->user vuole chattare
    struct device* q = utenti; // q mi trova il device destinatario , p punta al device mittente
    // ricevo utente in user_raw 
    ret = recv(sd,&len,sizeof(len),0);
    if (ret < 0){
        printf("Errore dovuto alla recv NEWCHAT\n");
        exit(-1);
    }
    
    dim = ntohs(len);
    ret = recv(sd,user_raw,dim,0);
    if (ret < 0){
        printf("Errore dovuto alla recv NEWCHAT\n");
        exit(-1);
    }
    // ricevo messaggio in buff
    ret = recv(sd,&len,sizeof(len),0);
    if (ret < 0){
        printf("Errore dovuto alla recv NEWCHAT\n");
        exit(-1);
    }
    dim = ntohs(len);
    ret = recv(sd,buff,dim,0);
    if (ret < 0){
        printf("Errore dovuto alla recv NEWCHAT\n");
        exit(-1);
    }

    // verifico se è destinatario online
    while ( q != NULL ){
        if (!strcmp(q->user,user_raw) && q->islog){
            break;
        }
        q = q->punt;
    }

    if (q){ // utente online -> invio ack (port) al mittente e messaggio+username al destinatario 
    
        //inio username mittente al destinatario
        dim = strlen(p->user)+1;
        len = htons(dim);
        ret = send(q->socket_tcp,&len,sizeof(len),0);
        if (ret <= 0){
            printf("SEND ha resetituito <= 0\n");
            exit(-1);
        }
        ret = send(q->socket_tcp,p->user,strlen(p->user)+1,0);
        if (ret <= 0){
            printf("SEND ha resetituito <= 0\n");
            exit(-1);
        }
        //invio msg al dest
        dim = strlen(buff)+1;
        len = htons(dim);
        ret = send(q->socket_tcp,&len,sizeof(len),0);
        if (ret <= 0){
            printf("SEND ha resetituito <= 0\n");
            exit(-1);
        }
        ret = send(q->socket_tcp,buff,strlen(buff)+1,0);
        if (ret <= 0){
            printf("SEND ha resetituito <= 0\n");
            exit(-1);
        }
        // invio ack (port) al mittente -> gli dico che destinatario online
        len = htons(q->port);          
        ret = send(sd,&len,sizeof(len),0);
        if (ret < 0){
            printf("ERRORE send dentro NEWCHAT\n");
            free(p);
        }
        return;
    }
    // salvo messaggio (buff) in file chat con '*'

    // aggiorno file di log
    logRefresh(user_raw);

    // invio ack con lo zero -> utente non online

    len = htons(0);                 //restituire 0 (utente destinatario offline)
    ret = send(sd,&len,sizeof(len),0);
    if (ret < 0){
        printf("ERRORE send dentro NEWCHAT\n");
        free(p);
        return;
    }

}

void sendNOTIFICA(int sd){
    char user_raw[25];
    struct device* q;    // scorre lista utenti online e cerca usr_raw

    ret = recv(sd,&len,sizeof(len),0);
    if (ret < 0){
        printf("Errore dovuto alla recv\n");
        exit(-1);
    }
    
    dim = ntohs(len);
    ret = recv(sd,user_raw,dim,0);
    if (ret < 0){
        printf("Errore dovuto alla recv\n");
        exit(-1);
    }

    q = utenti;
    while(q != NULL ){
        if (!strcmp(q->user,user_raw))
            break;
        q = q->punt;
    }

    if(!q || !q->islog){ // q non è mai stato loggato o comunque non è online
        return;
    }
    
    // l'utente q->user è online
    //gli notifco che l'utente p->user ha letto i suoi messaggi pendenti

    // informo della notifica
    len = htons(1001);
    ret = send(q->socket_tcp,&len,sizeof(len),0);
    if (ret < 0){
        printf("errore send nell invio di ack notifica\n");
    }

    // invio nome utente che ha letto i suoi messaggi
    dim = strlen(p->user)+1;
    len = htons(dim);
    ret = send(q->socket_tcp,&len,sizeof(len),0);
    if (ret < 0){
        printf("errore send nell invio di ack notifica\n");
    }

    ret = send(q->socket_tcp,p->user,dim,0);
    if (ret < 0){
        printf("errore send nell invio di ack notifica\n");
    }

}

void menu(){

    sleep(1);
    printf("\n\nSalve, Per interagire con il menu premere il numero relativo al comando:\n");
    printf("1) HELP -----> breve introduzione ai comandi\n");
    printf("2) LIST ------> mostra lista degli utenti online\n");
    printf("3) ESC ------> termina esecuzione server\n");
    printf("-\n");
}

void logRefresh(char destinatario[]){
    // aggiorno file di log -> (l'utente destinatario è offline quindi file di log dovrebbe contenere timestamp uscita o NULL 
    //                          + eventuale serie di utenti e messaggi non recapitati)
    FILE* log;
    char time_stamp[9];
    char delimiter[25];
    char mittente[25];
    int numero_int = 0;
    char numero_char[4];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf(time_stamp,"%d:%d:%d",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);      //time_stamp messaggio più recente

    sprintf(buff,"GENERAL/log-%s.txt",destinatario);   //meccanismo per aprire/creare log utente
    log = fopen(buff,"r+"); //apro in lettura per fare controllo
    if (log == NULL){
        log = fopen(buff,"w");
        printf("Creazione LOG-file di %s\n",destinatario);
        fclose(log);
        log = fopen(buff,"r+"); //apro in lettura per fare controllo
    }
    
    while( fscanf(log,"%s",delimiter) != EOF ){        // fino a che non finisce il file 
        if (!strcmp(delimiter,"<")){    // se trovi un carattere <
            fseek(log,strlen(delimiter),SEEK_CUR);
            fscanf(log,"%s",mittente);
            if (!strcmp(mittente,p->user)){ // se utente di quella riga è p->user ( punta a chi ha contattato il server(mittente) )
                fscanf(log,"%s",numero_char);
                fseek(log,-strlen(numero_char), SEEK_CUR);
                sscanf(numero_char,"%d",&numero_int);
                numero_int++;                               // incrementa il numero dei messaggi pendenti
                sprintf(numero_char,"%d",numero_int);
                fprintf(log,"%s",numero_char);
                fprintf(log," %s >",time_stamp);        //aggiorna time_stamp
                break;
            }
        }
    }  
    fclose(log);
    if (!numero_int){     // questo era il primo messaggio di p->user a destinatario non recapitato
    // aggiorno log con nuova riga
        log = fopen(buff,"a"); //buff contiene ancora nome file log
        sprintf(buff,"\n< %s 1 %s >    ",p->user,time_stamp);     //padding nel caso doppie cifre
        fprintf(log,"%s",buff);
        fclose(log);
    }
    
}

void OFFDEVICE(){       // p punta alla stuct del device che si sta disconnettendo

    p->islog = 0;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf(p->time_stamp,"%d:%d:%d",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);      //inserisco time_stamp logout
    close(p->socket_tcp);
    FD_CLR(p->socket_tcp,&master);

    return;
}
