#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define CONFIG_FILE "roteador.config"
#define ENLACE_FILE "enlace.config"
#define Controle 0
#define Dado 1

//se estiver debugando == 1 se n qualquer outra coisa
#define debugando 0

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
    //destino quer dizer qual roteador tem q ir e a saida é por onde vai passar a msg
    int destino;
    int saida;
    //quanto custa até a saida
    int custo;

    //se é vizinho
    int isVisinho;
    //quanto tempo faz q n manda o vetor distancia
    int rodadasSemResposta;

} Distancia;

typedef struct 
{
    //todos os vetores
    Distancia vetores[numRoteadores];
    
    pthread_mutex_t lock; //mutex

    
} VetoresDistancia;



//este roteador só tem q saber isso
typedef struct 
{
    //destino quer dizer qual roteador tem q ir e a saida é por onde vai passar a msg
    int destino;
   
    //quanto custa até a saida
    int custo;

} DistanciaRecebido;



// cada roteador vai mandar um destes para calcular o vetor distancia
typedef struct 
{
    //todos as distancias de um roteador, como vai ser só analizado n tem q ter lock, o que junta todos vai ter
    DistanciaRecebido vetores[numRoteadores];
    
} vetoresRecebidos;


//quando chegar um novo vetor distancia armazena aqui para processar
typedef struct 
{
    //vetores distancias recebidos mas n analizados, vetor de vetores
    vetoresRecebidos vetoresNaoAnalizados[numRoteadores];
    //vetor que controla se ja foi calculado, zero se deve ser analizado, 1 se ja foi
    int testados[numRoteadores];

    pthread_mutex_t lock; //mutex

    
} AnalizarVetores;



//filas globais

Fila filaEntrada;
Fila filaSaida;
int id;  //mudar vai receber do arquivo, ai abre outroa rquivo para pegar seu ip
int meuSocket;
//vetor que guarda a topografia
VetoresDistancia vetorDistancia;
//vetor com as topografias para analize
AnalizarVetores vetoresParaAnalize;


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
void sendMsg(Mensagem msg ){

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



//pega o topo da pilha da fila de saida
Mensagem getMsgFilaSaida(){
    
    //msg de erro, acho q pode dar erro, acho estranho como ta mas tava funcionando
    Mensagem vazio = {0};

    //tenta pegar o lock
    pthread_mutex_lock(&filaSaida.lock);


    //get lock
    if(filaSaida.tamanho == 0){

        pthread_mutex_unlock(&filaSaida.lock);
        return vazio;

    }

    //passou os testes, pega a mensagem

    Mensagem retorno = filaSaida.conteudo[0];
    
    //tira da lista zera os bytes
    memset(&filaSaida.conteudo[0], 0, sizeof(Mensagem));

    //faz voltar 1 pos
    for(int i = 1; i<filaSaida.tamanho;i++){
        filaSaida.conteudo[i-1] = filaSaida.conteudo[i];
    }

    //diminui o tamanho
    filaSaida.tamanho --;

    //libera o lock e tira do semafaro, semafaro tem q ter coisa

    pthread_mutex_unlock(&filaSaida.lock);

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
    printf("Origem: %d\nDestino: %d\nTipo: %s\nConteudo: %s",msg.origem, msg.destino,tipo,msg.conteudo);


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


//func para criar msg de dado
Mensagem criarMsgDados(){

    Mensagem newMsg;

    newMsg.origem = id;
    
    //o define n estava fncionando
    newMsg.tipo = Dado;


    int escolha;

    while(1){
        printf("Digite o Destino da Mensagem: \n");

        //faz um teste para ver se leu um valor inteiro, caso contrario usuario foi burro
        if (scanf("%d", &escolha) != 1) {
            printf("Entrada invalida!\n");
            //limpa o buffer do scanf
            while (getchar() != '\n');
            continue;
        }
        //isso é para debug tirar o + 1000
        if(escolha == id + 1000){
            printf("Destino igual a origem, escolha novamente\n"); 
            //limpa o buffer do scanf
            while (getchar() != '\n');
            continue;
        }
       

        newMsg.destino = escolha;
        while (getchar() != '\n'); //limpa o ENTER que sobra do scanf
        break;
    }

    printf("Digite o conteudo da mensagem: \n");
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

    return newMsg;
}




//abr o arquivo e pega o scokt
int pegaSocket(const char *filename, int idProcurar) {

    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("deu pao, n abriu o arquivo %s\n", filename);
        return -1;
    }

    int tempId, port;
    char ip[64];

    // olha cada linha no formato "id port ip", o 63 é para pegar o ip, mas vai ignorar se n for local
    while (fscanf(f, "%d %d %63s", &tempId, &port, ip) == 3) {
        if (tempId == idProcurar) {
            fclose(f);
            return port; //achou a porta
        }
    }




    fclose(f);
    return -1; // não achou o id
}





void zerarFilaTesta(){

    //marca como todos analizados pois n veio nenhum ainda
    pthread_mutex_lock(&vetoresParaAnalize.lock);

    for (int i = 0; i < numRoteadores; i++)
        vetoresParaAnalize.testados[i] = 1;

    pthread_mutex_unlock(&vetoresParaAnalize.lock);

}


//vai botar o vetor para analizar, passa um vetor distancia, e o roteador q mandou
void addVetorAnalize(int roteadorOrigem, vetoresRecebidos vetorAdicionar){

    pthread_mutex_lock(&vetoresParaAnalize.lock);

    //bota o no lugar do roteador q mandou
    vetoresParaAnalize.vetoresNaoAnalizados[roteadorOrigem-1] = vetorAdicionar;

    //marca como nao analizado
    vetoresParaAnalize.testados[roteadorOrigem-1] = 0;

    pthread_mutex_unlock(&vetoresParaAnalize.lock);


}




//só chamar com o lock obtido, só pra debug
void imprimirVetorDistancia(){

      for(int i =0; i< numRoteadores;i++){
        printf("\nRoteador %d que esta na pos %d do vetor:\n", i+1, i);
        printf("vizinho : %d, custo: %d\n", vetorDistancia.vetores[i].isVisinho,vetorDistancia.vetores[i].custo );
        printf("sainda: %d, tempo sem mandar o vetor: %d\n\n",vetorDistancia.vetores[i].saida, vetorDistancia.vetores[i].rodadasSemResposta);
        /*
        printf("Destino: %d, por onde vai a msg: %d\n",vetorDistancia.vetores[i].destino,vetorDistancia.vetores[i].saida);
        printf("Custo: %d, é visinho: %d, rodadas sem responder: %d",vetorDistancia.vetores[i].custo,vetorDistancia.vetores[i].isVisinho,vetorDistancia.vetores[i].rodadasSemResposta);
         */
        
    }

}





//theads ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void *theadFilaEntrada() {

    //printFila(filaEntrada);
   

    while (1){
        //tem coisa entao vai dar o get, new ´é a mensagem que chegou agora tem q tratar
        sem_wait(&filaEntrada.cheio);

        Mensagem newMensagem = getMsg();


        //msg pra printar pta mim
        if(newMensagem.tipo == Dado && newMensagem.destino == id){
            printMsgFormatada(newMensagem);
        }

        if (newMensagem.tipo == Controle && newMensagem.destino == id){

            //formatar a msg para passar nesse vetor
            vetoresRecebidos newVetor;

            //vai receber o custo para todos em ordem
            //cuida da string recebida da msg

            
            char msg[500];
            
            strncpy(msg, newMensagem.conteudo, sizeof(msg) - 1);
            msg[sizeof(msg) - 1] = '\0'; //garante terminação

            //contador
            int i = 0;

            // quebra a string por espaços
            char *token = strtok(msg, " ");

            //olhar onde devo colocar 
            while (token != NULL && i < numRoteadores) {
                newVetor.vetores[i].custo = atoi(token);  // converte pra int
                newVetor.vetores[i].destino = i + 1; //pq o roteador 1 fica na pos 0
                i++;
                token = strtok(NULL, " ");
            }

            

            //////////////////fazeeeeeeeeeeeeeeeeeeeeeeeeeeer

            addVetorAnalize(newMensagem.origem,newVetor);

        }
    
        if(newMensagem.destino != id){

            sendMsg(newMensagem);

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
        sem_wait(&filaSaida.cheio);

    
        Mensagem newMsg = getMsgFilaSaida();

        //vai pegar essa newMsg e botar ela no socket do roteador do ip, tem q abrir o arquvio
        //e ver o socket dele

        if(newMsg.destino == id){
            printf("capitei uma msg para mim");
        }
        


        int numSocketEnviar = pegaSocket("roteador.config", newMsg.destino);
        printf("numSocketEnviar: %d", numSocketEnviar );





        //aqui vai mandar pro socket
    }
    return NULL;
}


//cuida dos vetores distancias


void *theadVetorDistancia(){

    //zera o vetor de entrada dos outros
    zerarFilaTesta();

    //pega o lock

    pthread_mutex_lock(&vetorDistancia.lock);
   
    //cria os vetores distancia e zera eles
    
    for(int i = 0; i< numRoteadores; i++){

        //deixa em fila ou seja, o roteador 1 é o 0 e assim vai

        //ajusta ele mesmo
        if (i == id){
            //custo zero
            vetorDistancia.vetores[i].custo = 0;
            //o desino é ele
            vetorDistancia.vetores[i].destino = id;
            //saida fodase pq vai só usar localmente
            vetorDistancia.vetores[i].saida = -1;
            //é ele msm
            vetorDistancia.vetores[i].isVisinho = 0;
            //n importa
            vetorDistancia.vetores[i].rodadasSemResposta = 0;
        }
        else{

            vetorDistancia.vetores[i].custo = -1;
            vetorDistancia.vetores[i].destino = -1;
            vetorDistancia.vetores[i].saida = -1;
            //mudar dependendo do arquivo
            vetorDistancia.vetores[i].isVisinho = 0;
           //cada vizinho q n mandar por rodadada adiciona um, se chegar 3 caiu o enlace
            vetorDistancia.vetores[i].rodadasSemResposta = 0;

        }
    }
    
    //botei aqui esse }s

    //abre o arquivo

    FILE *f = fopen(ENLACE_FILE, "r");

    if (!f) {
        printf("Erro ao abrir o arquivo.\n");
    }

   
    int id1, id2, custo;
    int vizinho;
    while (fscanf(f, "%d %d %d", &id1, &id2, &custo) == 3) {

        //ve se é com ele
        if(id1 == id){
            
            vizinho = id2;
        }
        if (id2 == id){
            vizinho = id1;
        }

        //se tiver ele faz isso
        if(id == id1 || id == id2){

        // -1 pq ele ta armazenado em 1 atras
        vetorDistancia.vetores[vizinho - 1].custo = custo;
        vetorDistancia.vetores[vizinho -1].isVisinho = 1;

        //é para onde ele quer ir
        vetorDistancia.vetores[vizinho-1 ].destino = vizinho;

        //é o menor caminho até agora
        vetorDistancia.vetores[vizinho-1].saida = vizinho;
        
        }
        
    }

    

    fclose(f);

    //enviar vetores distancias e dar um sleep    
    
    sleep(1);

    if(debugando == 1){
        imprimirVetorDistancia();
    }
   

    pthread_mutex_unlock(&vetorDistancia.lock);

    //loop principal deve ser ativado a cada alguns momentos para fazerr os calculos dos ençaces
    while (1)
    {

        
        //pega o lock, faz os calculos para ver se tem uma distancia menor, se mudar manda e volta a dormir

        //adiciona +1 para a demora de todos, os que mandarem zera, dps analiza os q fiaram com 3 no final mas so os viinhos

        //pega o lock do vetor distancia principal
        pthread_mutex_lock(&vetorDistancia.lock);
        //se mudar vira 1
        int mudou = 0;

        //add +1 em todos os tempo dos vizinhos
        for(int i =0; i< numRoteadores;i++){
            if(vetorDistancia.vetores[i].isVisinho == 1){
                vetorDistancia.vetores[i].rodadasSemResposta ++;
            }
        }



        //pega o lock do vetor dos que chegaram
        pthread_mutex_lock(&vetoresParaAnalize.lock);
        
        //ve se ja foi testado
        for (int i = 0; i < numRoteadores; i++) {

            if (vetoresParaAnalize.testados[i] == 0) {

                //percorre todos os destinos recebidos do roteador i
                for (int d = 0; d < numRoteadores; d++) {

                    int destino = vetoresParaAnalize.vetoresNaoAnalizados[i].vetores[d].destino;
                    int custoRecebido = vetoresParaAnalize.vetoresNaoAnalizados[i].vetores[d].custo;

                    //ignora entradas inválidas
                    if (destino <= 0 || custoRecebido < 0)
                        continue;

                    //custo até o roteador que enviou, pra adicionar dps
                    int custoAteVizinho = vetorDistancia.vetores[i].custo;

                    //reinicia o tempo que n mando msg
                    vetorDistancia.vetores[i].rodadasSemResposta = 0;

                    //se o vizinho estiver inacessível, pula
                    if (custoAteVizinho < 0)
                        continue;

                    //custo total até o destino via esse vizinho
                    int novoCusto = custoAteVizinho + custoRecebido;

                    //se o destino ainda não tem rota, ou achou uma melhor
                    if (vetorDistancia.vetores[destino - 1].custo < 0 ||
                        novoCusto < vetorDistancia.vetores[destino - 1].custo) {

                        vetorDistancia.vetores[destino - 1].custo = novoCusto;
                        vetorDistancia.vetores[destino - 1].saida = i + 1;
                        mudou = 1;
                    }
                }

                //marca como testado (já analisado)
                vetoresParaAnalize.testados[i] = 1;
            }
        }

        for(int i =0; i<numRoteadores; i++){

            if(vetorDistancia.vetores[i].isVisinho == 1 && vetorDistancia.vetores[i].rodadasSemResposta == 3){
                //caiu o enlace
                vetorDistancia.vetores[i].isVisinho = 0;
                vetorDistancia.vetores[i].custo = -1;
                vetorDistancia.vetores[i].saida = -1;

                //marca q mudou
                mudou = 1;
            }

        }

        pthread_mutex_unlock(&vetoresParaAnalize.lock);
        

        //cria o txt q vai mandar na msg d controle
        char stringControle[500] = "";
        char temp[32];  //guarda o numero antes d botar

        for (int aux = 0; aux < numRoteadores; aux++) {
            //converte o custo em str e bota um espaço
            snprintf(temp, sizeof(temp), "%d ", vetorDistancia.vetores[aux].custo);
            strcat(stringControle, temp);
        }
        


        if(mudou == 1){
            //pra todos os roteadores
            for(int roteadores = 0; roteadores<numRoteadores;roteadores++){
                //se for vizinho
                if(vetorDistancia.vetores[roteadores].isVisinho == 1){

                    Mensagem novaMsgControle;
                    novaMsgControle.origem = id;
                    novaMsgControle.destino = roteadores;
                    novaMsgControle.tipo = Controle;
                    //vai botar no campo da nova msg e cortar o tamanho
                    strncpy(novaMsgControle.conteudo, stringControle, sizeof(novaMsgControle.conteudo) - 1);
                    //marca o ultimo char como fim do texto
                    novaMsgControle.conteudo[sizeof(novaMsgControle.conteudo) - 1] = '\0';

                    //bota na fila d saida
                    sendMsg(novaMsgControle);

                }

            }
        }


        pthread_mutex_unlock(&vetorDistancia.lock);

        sleep(1);
       
    }
    
    //estava aqui
    //}
    
    
}


//terminal é a main
int main(int argc, char *argv[])
{

    id = atoi(argv[1]); //converte o argumento para inteiro, isso   q faz ter q passar argumento
    //tem q passar certo se n da pau

    meuSocket = pegaSocket("roteador.config", id);

    printf("socket: %d", meuSocket);

    initFilas();




    pthread_t tEntrada, tSaida, tVetorDistancia;




    pthread_create(&tEntrada, NULL, theadFilaEntrada, NULL);
    pthread_create(&tSaida, NULL, theadFSaida, NULL);
    pthread_create(&tVetorDistancia, NULL, theadVetorDistancia, NULL);
    

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
        printf("Digite o numero da opcao desejada: ");
        

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

                Mensagem novaMensagem = criarMsgDados();

                sendMsg(novaMensagem);

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
