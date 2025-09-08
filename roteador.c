#include <stdio.h>
#include <string.h>


#define Controle 0;
#define Dado 1;

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
void addMsg(Fila fila, Mensagem msg ){

    //get lock
    if(fila.tamanho >= 15){
        printf("fila cheia");

        //deixa lock
        return;
    }

   
    fila.conteudo[fila.tamanho] = msg;
    fila.tamanho ++;
    //deixa lock
    return;
}

//passar ponteiro
Mensagem getMensagem(Fila fila){

    //get lock
    if(fila.tamanho == 0){
        //lost lock
        return;

    }

    fila.tamanho --;

    Mensagem retorno = fila.conteudo[fila.tamanho];
    
    //tira da lista zera os bytes
    memset(&fila.conteudo[fila.tamanho], 0, sizeof(Mensagem));

    return retorno;
    

}


//Mensagem minhaMensagem;

//terminal é a main
int main(){

    //testar coisas da lista 

    
}
