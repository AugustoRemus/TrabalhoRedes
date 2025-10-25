

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define Controle 0
#define Dado 1

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


//filas globais

Fila filaEntrada;
Fila filaSaida;
int id= 1;  //mudar vai receber do arquivo, ai abre outroa rquivo para pegar seu ip

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



//passa o global, pega o topo da fila
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

    printf("printando Mensagem:\n");
    printf("Origem: %d\nDestino: %d\nTipo: %d\nConteudo: %s",msg.origem, msg.destino, msg.tipo,msg.conteudo);


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

     printf("Digite o conteúdo da mensagem: ");
    fgets(newMsg.conteudo, sizeof(newMsg.conteudo), stdin);

    //tira o '\n' que o fgets pode deixar no final
    newMsg.conteudo[strcspn(newMsg.conteudo, "\n")] = '\0';

    return newMsg;
}











//theads ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void *theadFilaEntrada() {

    printFila(filaEntrada);

    while (1){
        //tem coisa entao vai dar o get, new ´é a mensagem que chegou agora tem q tratar
        sem_wait(&filaEntrada.cheio);
        Mensagem newMensagem = getMsg();
        printMsg(newMensagem);
        //só ta pegando pra tirar a msg

        //descobrir como fazer isso funcionar, olhar no tranbalho, n to entendendo como ele pega o id dele
        //if (newMensagem.destino == )

    }
    return NULL;
}

void *theadFSaida() {

     while (1){
        //trhead ligada
    }
    return NULL;
}



//terminal é a main
int main(){

    initFilas();

    pthread_t tEntrada, tSaida;

  
    /*  
    //ta criando o resto com lixo ai printa cagado se n for completa
    Mensagem msg1;
    Mensagem msg2;
    Mensagem msg3;
    
    strcpy(msg1.conteudo, "um");
    strcpy(msg2.conteudo, "dos");
    strcpy(msg3.conteudo, "tres");

    addMsg(msg1);
    addMsg(msg2);
    addMsg(msg3);
    */

    pthread_create(&tEntrada, NULL, theadFilaEntrada, NULL);
    pthread_create(&tSaida, NULL, theadFSaida, NULL);

    //menu, esse id vai mudar ta aqui só pra n dar bug, ele é o global e vai passar d parametro
    //quando inicia o arquivo
    int id = 1;
    int escolha;

    while (1)
    {
        printf("\n===============================\n");
        printf(" Bem-vindo à interface do roteador: %d\n", id);
        printf("===============================\n");
        printf("1 - Ver status da conexão\n");
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

                //enviar socket novamensegem.destino

                print("Mensagem enviada com Sucesso");
                
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
