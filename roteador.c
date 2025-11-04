#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define CONFIG_FILE "roteador.config"
#define Controle 0
#define Dado 1

#define numRoteadores 4


#define tamanhoMaximoFila 15


typedef struct{
    int tipo;//tipo de msg
    int origem;//quem enviou
    int destino;//qual é o destino
    // para fazer rota calcular no inicio
    char conteudo[500]; //texto que a msg vai mandar
} Mensagem;


typedef struct{

    int tamanho;
    Mensagem conteudo[tamanhoMaximoFila];

  
    pthread_mutex_t lock; //protege o acesso a fila
    sem_t cheio;  //fica olhando pra n ficar executando

} Fila;

typedef struct 
{
    int destino;
    int saida;
    int custo;

} Distancia;

typedef struct 
{
    //todos os vetores
    Distancia vetores[numRoteadores];
    pthread_mutex_t lock; //mutex

    
} VetoresDistancia;



//filas globais

Fila filaEntrada;
Fila filaSaida;
int id;  //mudar vai receber do arquivo, ai abre outroa rquivo para pegar seu ip
int meuSocket;
VetoresDistancia vetorDistancia;

void initFilas() {

    // filaEntrada
    filaEntrada.tamanho = 0;
    memset(filaEntrada.conteudo, 0, sizeof(filaEntrada.conteudo));
    pthread_mutex_init(&filaEntrada.lock, NULL);
    sem_init(&filaEntrada.cheio, 0, 0);


    // filaSaida
    filaSaida.tamanho = 0;
    memset(filaSaida.conteudo, 0, sizeof(filaSaida.conteudo));
    pthread_mutex_init(&filaSaida.lock, NULL);
    sem_init(&filaSaida.cheio, 0, 0);

}


//passar como ponteiro
//passar o semaforo para bloquear na hora certa !!!!!!!!!!!!!!! tem q mudar pra multithead
//ou sempre pegar o lock, se para daria pra controlar melhor
void addMsg(Mensagem msg ){

    pthread_mutex_lock(&filaEntrada.lock);
    

    //testa se a fila ta cheioa
    if(filaEntrada.tamanho >= tamanhoMaximoFila){
        printf("fila cheia");

        pthread_mutex_unlock(&filaEntrada.lock);
        return;
    }

   //muda  as var da fila
    filaEntrada.conteudo[filaEntrada.tamanho] = msg;
    filaEntrada.tamanho ++;

    //libera po lock e add o semafaro
    sem_post(&filaEntrada.cheio);
    pthread_mutex_unlock(&filaEntrada.lock);
    
    return;
}

//passar como ponteiro
//passar o semaforo para bloquear na hora certa !!!!!!!!!!!!!!! tem q mudar pra multithead
//ou sempre pegar o lock, se para daria pra controlar melhor
void encaminharMsg(Mensagem msg ){

    pthread_mutex_lock(&filaSaida.lock);
    

    //testa se a fila ta cheioa
    if(filaSaida.tamanho >= tamanhoMaximoFila){
        printf("fila cheia");

        pthread_mutex_unlock(&filaSaida.lock);
        return;
    }

   //muda  as var da fila
    filaSaida.conteudo[filaSaida.tamanho] = msg;
    filaSaida.tamanho ++;

    //libera po lock e add o semafaro
    sem_post(&filaSaida.cheio);
    pthread_mutex_unlock(&filaSaida.lock);
    
    return;
}




//passa o global, pega o topo da fila, quando adiciona só taca no topo
//Mensagem m = getMensagem(&minhafila);
Mensagem getMsg(){
    
    //msg de erro
    Mensagem vazio = {0};

    //tenta pegar o lock
    pthread_mutex_lock(&filaEntrada.lock);


    //get lock
    if(filaEntrada.tamanho == 0){

        pthread_mutex_unlock(&filaEntrada.lock);
        return vazio;

    }

    //passou os testes, pega a mensagem

    Mensagem retorno = filaEntrada.conteudo[0];
    
    //tira da lista zera os bytes
    memset(&filaEntrada.conteudo[0], 0, sizeof(Mensagem));

    //faz voltar 1 pos
    for(int i = 1; i<filaEntrada.tamanho;i++){
        filaEntrada.conteudo[i-1] = filaEntrada.conteudo[i];
    }

    //diminui o tamanho
    filaEntrada.tamanho --;

    //libera o lock e tira do semafaro, semafaro tem q ter coisa

    //o semaforo só muda na thead
    //sem_wait(&filaEntrada.cheio);

    pthread_mutex_unlock(&filaEntrada.lock);

    return retorno;
    

}

void printMsg(Mensagem msg){

      char *tipo;
    if (msg.tipo == Controle){
        tipo = "Controle";
    }
    else{
        tipo = "dado";
    }

    printf("printando Mensagem:\n");
    printf("Origem: %d\nDestino: %d\nTipo: %d\nConteudo: %s",msg.origem, msg.destino,tipo,msg.conteudo);


}


//pra imprimir msg d texto
void printMsgFormatada(Mensagem msg){

    printf("Nova Mensagem do roteador: %d :\n", msg.origem);
    printf("%s",msg.conteudo);


}




void printFila(Fila fila){

    //se der pau botar o lock
    pthread_mutex_lock(&fila.lock);
    for(int i =0; i<fila.tamanho;i++){
        printf("Mensagem na posicao %d: %s \n", i, fila.conteudo[i].conteudo);
    }
    pthread_mutex_unlock(&fila.lock);
}



Mensagem criarMsg(){

    Mensagem newMsg;
    newMsg.destino = id;
    


    int escolha;

    while(1){
        printf("Digite o Destino da Mensagem: \n");

        //faz um teste para ver se leu um valor inteiro, caso contrario usuario foi burro
        if (scanf("%d", &escolha) != 1) {
            printf("Entrada inválida!\n");
            //limpa o buffer do scanf
            while (getchar() != '\n');
            continue;
        }
        if(escolha == id){
            printf("Destino igual a origem, escolha novamente\n"); 
            //limpa o buffer do scanf
            while (getchar() != '\n');
            continue;
        }
       

        newMsg.destino = escolha;
        while (getchar() != '\n'); //limpa o ENTER que sobra do scanf
        break;
    }

    printf("Digite o conteúdo da mensagem: \n");
    while(1){

        //pega a msg e salva
        fgets(newMsg.conteudo, sizeof(newMsg.conteudo), stdin);

        //tira o '\n' que o fgets pode deixar no final
        newMsg.conteudo[strcspn(newMsg.conteudo, "\n")] = '\0';

        //se for maior q 100 manda fazer dnv
        if(strlen(newMsg.conteudo) > 100){

            printf("Mensagem muito grande, tente novamente\n");
            while (getchar() != '\n');
            continue;
        }

        break;
    }    

    newMsg.tipo = Dado;
    return newMsg;
}


//abr o arquivo e pega o scokt
int pegaSocket(const char *filename) {

    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("deu pao, n abriu o arquivo %s\n", filename);
        return -1;
    }

    int tempId, port;
    char ip[64];

    // olha cada linha no formato "id port ip", o 63 é para pegar o ip, mas vai ignorar se n for local
    while (fscanf(f, "%d %d %63s", &tempId, &port, ip) == 3) {
        if (tempId == id) {
            fclose(f);
            return port; //achou a porta
        }
    }




    fclose(f);
    return -1; // não achou o id
}





//theads ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void *theadFilaEntrada() {

    printFila(filaEntrada);

    while (1){
        //tem coisa entao vai dar o get, new ´é a mensagem que chegou agora tem q tratar
        sem_wait(&filaEntrada.cheio);

        Mensagem newMensagem = getMsg();


        //msg pra printar pta mim
        if(newMensagem.tipo == Dado && newMensagem.destino == id){
            printMsgFormatada(newMensagem);
        }

        //ou é  de controle pra mim ou é pra outra pessoa
        else{
            //n é pra mim a msg
            if(newMensagem.destino != id){
                encaminharMsg(newMensagem);
            }


            //realizar calculo vetor distancia

        

        }
        //printMsg(newMensagem);
        //só ta pegando pra tirar a msg

        //descobrir como fazer isso funcionar, olhar no tranbalho, n to entendendo como ele pega o id dele
        //if (newMensagem.destino == )

    }
    return NULL;
}


//aqui vai mandar as trheadas
void *theadFSaida() {

     while (1){
        //trhead ligada
    }
    return NULL;
}



//terminal é a main
int main(int argc, char *argv[])
{

    id = atoi(argv[1]); //converte o argumento para inteiro, isso   q faz ter q passar argumento
    //tem q passar certo se n da pau

    meuSocket = pegaSocket("roteador.config");

    printf("socket: %d", meuSocket);

    initFilas();

    //criar vetores distancia, zera todos menos ele mesmo

    pthread_mutex_lock(&vetorDistancia.lock);
    
    for(int i = 0; i< numRoteadores; i++){

        //ajusta ele mesmo
        if (i == id){
            //custo zero
            vetorDistancia.vetores[i].custo = 0;
            //o desino é ele
            vetorDistancia.vetores[i].destino = id;
            //saida fodase pq vai só usar localmente
            vetorDistancia.vetores[i].saida = -1;

        }
        else{

            vetorDistancia.vetores[i].custo = -1;
            vetorDistancia.vetores[i].destino = -1;
            vetorDistancia.vetores[i].saida = -1;

        }
        
    pthread_mutex_unlock(&vetorDistancia.lock);

        


    }



    pthread_t tEntrada, tSaida;




    pthread_create(&tEntrada, NULL, theadFilaEntrada, NULL);
    pthread_create(&tSaida, NULL, theadFSaida, NULL);

    //menu, esse id vai mudar ta aqui só pra n dar bug, ele é o global e vai passar d parametro
    //quando inicia o arquivo
    //int id = 1;
    int escolha;

    while (1)
    {
        printf("\n===============================\n");
        printf(" Bem-vindo a interface do roteador: %d\n", id);
        printf("===============================\n");
        printf("1 - Ver status da conexao\n");
        printf("2 - Enviar Mensagem\n");
        printf("3 - ESCOLHA3\n");
        printf("4 - ESCOLHA4\n");
        printf("0 - Sair\n");
        printf("-------------------------------\n");
        printf("Digite o numero da opção desejada: ");
        

        //faz um teste para ver se leu um valor inteiro, caso contrario usuario foi burro
        if (scanf("%d", &escolha) != 1) {
            printf("Entrada inválida!\n");
            //limpa o buffer do scanf
            while (getchar() != '\n');
            continue;
        }
        printf("-------------------------------\n");
        switch (escolha)
        {
            case 1:
                printf("Status: Conectado e estável.\n");
                break;
            case 2:
                printf("Enviando Mensagem\n");

                Mensagem novaMensagem = criarMsg();

                encaminharMsg(novaMensagem);

                char *tipo;
                if (novaMensagem.tipo == Controle){
                    tipo = "Controle";
                }
                else{
                    tipo = "dado";
                }

                printf("Mensagem enviada do roteador: %d do tipo %s com destino: %d\n", id,tipo,novaMensagem.destino);
                

                printMsg(novaMensagem);
                
                break;


            case 3:
                printf(".\n");
                break;
            case 4:
                printf(".\n");
                break;
            case 0:
                printf("Saindo...\n");
                return 0;
            default:
                printf("Opção inválida! Tente novamente.\n");
                break;

            }
    
        }
    
    
    
}
