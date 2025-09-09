#include <stdio.h>
#include <string.h>


#define Controle 0
#define Dado 1

typedef struct{
    int tipo;//tipo de msg
    int origem;//quem enviou
    int destino;//qual é o destino
    // para fazer rota calcular no inicio
    char conteudo[500]; //texto que a msg vai mandar
} Mensagem;


typedef struct{

    int tamanho;
    Mensagem conteudo[15];

} Fila;


Fila criarFila(){

    Fila minhafila;

    minhafila.tamanho = 0;

    //zera os bytes
    memset(minhafila.conteudo, 0, sizeof(minhafila.conteudo));
   
    
    return minhafila;

}

//passar como ponteiro
//passar o semaforo para bloquear na hora certa !!!!!!!!!!!!!!! tem q mudar pra multithead
//ou sempre pegar o lock, se para daria pra controlar melhor
void addMsg(Fila *fila, Mensagem msg ){

    //get lock
    if(fila->tamanho >= 15){
        printf("fila cheia");

        //deixa lock
        return;
    }

   
    fila->conteudo[fila->tamanho] = msg;
    fila->tamanho ++;
    //deixa lock
    return;
}

//passar ponteiro
//Mensagem m = getMensagem(&minhafila);
Mensagem getMsg(Fila *fila){

    //get lock
    if(fila->tamanho == 0){
        //lost lock
        Mensagem vazio = {0};
        return vazio;

    }

  

    Mensagem retorno = fila->conteudo[0];
    
    //tira da lista zera os bytes
    memset(&fila->conteudo[0], 0, sizeof(Mensagem));

    //faz voltar 1 pos
    for(int i = 1; i<fila->tamanho;i++){
        fila->conteudo[i-1] = fila->conteudo[i];
    }

    fila->tamanho --;
    //deixar lock
    return retorno;
    

}


void printFila(Fila fila){
    for(int i =0; i<fila.tamanho;i++){
        printf("Mensagem na posicao %d: %s \n", i, fila.conteudo[i].conteudo);
    }
}


//Mensagem minhaMensagem;

//terminal é a main
int main(){

    Fila minhafila = criarFila();

    Mensagem msg1;
    Mensagem msg2;
    Mensagem msg3;
    
    strcpy(msg1.conteudo, "um");
    strcpy(msg2.conteudo, "dos");
    strcpy(msg3.conteudo, "tres");

    addMsg(&minhafila, msg1);
    addMsg(&minhafila, msg2);
    addMsg(&minhafila, msg3);
     addMsg(&minhafila, msg3);


    printFila(minhafila);

    Mensagem tirar = getMsg(&minhafila);
    tirar = getMsg(&minhafila);
    //testar coisas da lista 

     printFila(minhafila);
    
}
