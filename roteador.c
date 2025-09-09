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


//errado, ta uma pilha
Mensagem getMsg(Fila *fila){

    //get lock
    if(fila->tamanho == 0){
        //lost lock
        Mensagem vazio = {0};
        return vazio;

    }

    fila->tamanho --;

    Mensagem retorno = fila->conteudo[fila->tamanho];
    
    //tira da lista zera os bytes
    memset(&fila->conteudo[fila->tamanho], 0, sizeof(Mensagem));

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
    addMsg(&minhafila, msg1);


    printFila(minhafila);

    Mensagem tirar = getMsg(&minhafila);
    //testar coisas da lista 

     printFila(minhafila);
     //ta errado!!!
}
