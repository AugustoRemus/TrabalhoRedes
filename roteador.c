#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define Controle 0
#define Dado 1

// Novas constantes/configurações
#define MAX_NODES 10
#define MAX_NEIGHBORS 8
#define INF 9999
// intervalo de envio do vetor distância em segundos (configurável)
#define DIST_SEND_INTERVAL 5

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


// ----------------- Novas estruturas: vetor-distância e tabela de roteamento -----------------

// vetor-distância completo (do roteador ou recebido)
typedef struct {
    int owner_id;                    // id do roteador que originou este vetor
    int cost[MAX_NODES];             // custo para cada destino (tamanho fixo MAX_NODES)
    time_t timestamp;                // quando foi recebido/atualizado
} DistanceVector;

// vetor-distância recebido de um vizinho; mantido enquanto reachable
typedef struct {
    int neighbor_id;
    int reachable;                   // 1 = alcançável, 0 = não
    DistanceVector dv;
} NeighborDV;

// entrada da tabela de roteamento
typedef struct {
    int destination;
    int next_hop;
    int cost;
} RouteEntry;

// tabela de roteamento simples
typedef struct {
    RouteEntry entries[MAX_NODES];
    int size;
} RoutingTable;

// ----------------- Estado global (simples) -----------------
int my_id = 0;
int num_nodes_configured = 5; // ajuste conforme topologia (<= MAX_NODES)

int my_distance_vector[MAX_NODES]; // vetor distância próprio
NeighborDV neighborDVs[MAX_NEIGHBORS];
int neighborCount = 0;
RoutingTable rtable;

pthread_t sender_thread;
int sender_running = 1;
pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
// novo mutex para serializar acesso ao terminal (impressão + leitura)
pthread_mutex_t io_lock = PTHREAD_MUTEX_INITIALIZER;

// ----------------- Funções utilitárias -----------------

void init_state(int id){
    my_id = id;
    // inicializa meu vetor com INF exceto para mim
    for(int i=0;i<MAX_NODES;i++) my_distance_vector[i] = INF;
    my_distance_vector[my_id] = 0;

    // inicializa neighbors
    for(int i=0;i<MAX_NEIGHBORS;i++){
        neighborDVs[i].neighbor_id = -1;
        neighborDVs[i].reachable = 0;
        neighborDVs[i].dv.owner_id = -1;
        for(int j=0;j<MAX_NODES;j++) neighborDVs[i].dv.cost[j] = INF;
    }

    // inicializa tabela de roteamento vazia
    rtable.size = 0;
    for(int i=0;i<MAX_NODES;i++){
        rtable.entries[i].destination = i;
        rtable.entries[i].next_hop = -1;
        rtable.entries[i].cost = INF;
    }
    rtable.entries[my_id].next_hop = my_id;
    rtable.entries[my_id].cost = 0;
}

// adiciona um vizinho (marca alcançável). Não cria canais reais, apenas estado.
void addNeighbor(int neighbor_id){
    pthread_mutex_lock(&state_lock);
    if(neighborCount >= MAX_NEIGHBORS){
        pthread_mutex_unlock(&state_lock);
        return;
    }
    // procura vaga
    for(int i=0;i<MAX_NEIGHBORS;i++){
        if(neighborDVs[i].neighbor_id == -1){
            neighborDVs[i].neighbor_id = neighbor_id;
            neighborDVs[i].reachable = 1;
            neighborDVs[i].dv.owner_id = neighbor_id;
            neighborDVs[i].dv.timestamp = time(NULL);
            // custo direto inicial: vamos supor 1 para vizinho direto (exemplo)
            for(int j=0;j<MAX_NODES;j++) neighborDVs[i].dv.cost[j] = INF;
            neighborDVs[i].dv.cost[neighbor_id] = 0;
            neighborCount++;
            break;
        }
    }
    pthread_mutex_unlock(&state_lock);
}

// registrar recebimento de um vetor distância de um vizinho
void receiveDistanceVector(int from_id, int costs[]){
    pthread_mutex_lock(&state_lock);
    // procura registro do vizinho
    int idx = -1;
    for(int i=0;i<MAX_NEIGHBORS;i++){
        if(neighborDVs[i].neighbor_id == from_id){
            idx = i;
            break;
        }
    }
    if(idx == -1){
        // tenta inserir novo vizinho
        for(int i=0;i<MAX_NEIGHBORS;i++){
            if(neighborDVs[i].neighbor_id == -1){
                neighborDVs[i].neighbor_id = from_id;
                neighborDVs[i].reachable = 1;
                idx = i;
                neighborCount++;
                break;
            }
        }
    }
    if(idx == -1){
        pthread_mutex_unlock(&state_lock);
        return; // sem espaço para novos vizinhos
    }

    neighborDVs[idx].reachable = 1;
    neighborDVs[idx].dv.owner_id = from_id;
    neighborDVs[idx].dv.timestamp = time(NULL);
    for(int j=0;j<MAX_NODES;j++) neighborDVs[idx].dv.cost[j] = costs[j];

    // imprimir vetor recebido
    printf("[RECEBIDO] Vetor de %d (timestamp %ld):", from_id, (long)neighborDVs[idx].dv.timestamp);
    for(int j=0;j<num_nodes_configured;j++){
        printf(" %d", neighborDVs[idx].dv.cost[j]);
    }
    printf("\n");

    pthread_mutex_unlock(&state_lock);
}

// thread que periodicamente envia o vetor distância para cada vizinho (simulação: apenas imprime)
void *sender_loop(void *arg){
    (void)arg;
    while(sender_running){
        sleep(DIST_SEND_INTERVAL);

        // copia o estado relevante sob state_lock para evitar segurar locks durante printf
        int local_neighborCount;
        int local_mydv[MAX_NODES];
        int local_neighbors[MAX_NEIGHBORS];
        int local_neighbor_reachable[MAX_NEIGHBORS];

        pthread_mutex_lock(&state_lock);
        local_neighborCount = neighborCount;
        for(int i=0;i<MAX_NEIGHBORS;i++){
            local_neighbors[i] = neighborDVs[i].neighbor_id;
            local_neighbor_reachable[i] = neighborDVs[i].reachable;
        }
        for(int i=0;i<num_nodes_configured;i++) local_mydv[i] = my_distance_vector[i];
        pthread_mutex_unlock(&state_lock);

        // imprimir de forma exclusiva (serializada) para não misturar com o prompt
        pthread_mutex_lock(&io_lock);
        printf("[ENVIO] Enviando meu vetor-distancia para %d vizinhos (interval %d s)\n", local_neighborCount, DIST_SEND_INTERVAL);
        // imprime meu DV:
        printf("  Meu DV:");
        for(int i=0;i<num_nodes_configured;i++){
            int c = local_mydv[i];
            if(c>=INF) printf(" INF"); else printf(" %d", c);
        }
        printf("\n");
        for(int i=0;i<MAX_NEIGHBORS;i++){
            if(local_neighbors[i] != -1 && local_neighbor_reachable[i]){
                printf("  -> para vizinho %d: [", local_neighbors[i]);
                for(int j=0;j<num_nodes_configured;j++){
                    int c = local_mydv[j];
                    if(c>=INF) printf(" INF"); else printf(" %d", c);
                }
                printf(" ]\n");
            }
        }
        fflush(stdout);
        pthread_mutex_unlock(&io_lock);
    }
    return NULL;
}

// imprime os últimos vetores recebidos dos vizinhos
void printReceivedVectors(){
    // copia os dados sob state_lock
    int local_neighbor_ids[MAX_NEIGHBORS];
    int local_reachable[MAX_NEIGHBORS];
    time_t local_timestamps[MAX_NEIGHBORS];
    int local_costs[MAX_NEIGHBORS][MAX_NODES];

    pthread_mutex_lock(&state_lock);
    for(int i=0;i<MAX_NEIGHBORS;i++){
        local_neighbor_ids[i] = neighborDVs[i].neighbor_id;
        local_reachable[i] = neighborDVs[i].reachable;
        local_timestamps[i] = neighborDVs[i].dv.timestamp;
        for(int j=0;j<num_nodes_configured;j++) local_costs[i][j] = neighborDVs[i].dv.cost[j];
    }
    pthread_mutex_unlock(&state_lock);

    // imprime de forma exclusiva
    pthread_mutex_lock(&io_lock);
    printf("== Vetores recebidos de vizinhos ==\n");
    for(int i=0;i<MAX_NEIGHBORS;i++){
        if(local_neighbor_ids[i] != -1 && local_reachable[i]){
            printf("Vizinho %d (last %ld):", local_neighbor_ids[i], (long)local_timestamps[i]);
            for(int j=0;j<num_nodes_configured;j++){
                int c = local_costs[i][j];
                if(c>=INF) printf(" INF"); else printf(" %d", c);
            }
            printf("\n");
        }
    }
    fflush(stdout);
    pthread_mutex_unlock(&io_lock);
}

// imprime meu vetor distância (antes de enviar)
void printMyDistanceVector(){
    int local_mydv[MAX_NODES];
    pthread_mutex_lock(&state_lock);
    for(int i=0;i<num_nodes_configured;i++) local_mydv[i] = my_distance_vector[i];
    int local_id = my_id;
    pthread_mutex_unlock(&state_lock);

    pthread_mutex_lock(&io_lock);
    printf("== Meu vetor-distancia (id=%d) ==\n", local_id);
    for(int i=0;i<num_nodes_configured;i++){
        int c = local_mydv[i];
        if(c>=INF) printf(" INF"); else printf(" %d", c);
    }
    printf("\n");
    fflush(stdout);
    pthread_mutex_unlock(&io_lock);
}

// função simples para atualizar tabela de roteamento a partir dos vetores recebidos
void recomputeRoutingTable(){
    pthread_mutex_lock(&state_lock);

    // inicializa entradas
    for(int d=0; d<num_nodes_configured; d++){
        rtable.entries[d].destination = d;
        rtable.entries[d].cost = INF;
        rtable.entries[d].next_hop = -1;
    }
    rtable.entries[my_id].cost = 0;
    rtable.entries[my_id].next_hop = my_id;

    // relax usando vetores dos vizinhos (assume custo do link para vizinho = 1)
    for(int i=0;i<MAX_NEIGHBORS;i++){
        if(neighborDVs[i].neighbor_id != -1 && neighborDVs[i].reachable){
            int nh = neighborDVs[i].neighbor_id;
            int cost_to_nh = 1; // ajustar se tiver custo de link real
            for(int dest=0; dest<num_nodes_configured; dest++){
                int cost_via_nh = neighborDVs[i].dv.cost[dest];
                if(cost_via_nh >= INF) continue;
                int total = cost_to_nh + cost_via_nh;
                if(total < rtable.entries[dest].cost){
                    rtable.entries[dest].cost = total;
                    rtable.entries[dest].next_hop = nh;
                }
            }
        }
    }

    pthread_mutex_unlock(&state_lock);
}
// ...existing code...
// imprime tabela de roteamento
void printRoutingTable(){
    RouteEntry local_entries[MAX_NODES];
    int local_num = num_nodes_configured;
    pthread_mutex_lock(&state_lock);
    for(int i=0;i<local_num;i++) local_entries[i] = rtable.entries[i];
    pthread_mutex_unlock(&state_lock);

    pthread_mutex_lock(&io_lock);
    printf("== Tabela de Roteamento ==\n");
    printf("Destino | NextHop | Custo\n");
    for(int i=0;i<local_num;i++){
        int nh = local_entries[i].next_hop;
        int cost = local_entries[i].cost;
        if(cost>=INF) printf("%7d | %7s | %4s\n", i, " - ", "INF");
        else printf("%7d | %7d | %4d\n", i, nh, cost);
    }
    fflush(stdout);
    pthread_mutex_unlock(&io_lock);
}

// ----------------- Programa principal (exemplo de uso) -----------------
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

    init_state(0); // id 0 para este roteador (ajustar conforme necessário)
    num_nodes_configured = 5; // ex: rede com 5 nós (0..4)

    // Exemplo: adiciona dois vizinhos (IDs 1 e 2)
    addNeighbor(1);
    addNeighbor(2);

    // seta meu vetor distância (exemplo)
    for(int i=0;i<num_nodes_configured;i++) my_distance_vector[i] = INF;
    my_distance_vector[my_id] = 0;
    my_distance_vector[1] = 1; // custo direto para vizinho 1
    my_distance_vector[2] = 1; // custo direto para vizinho 2

    // start sender thread
    pthread_create(&sender_thread, NULL, sender_loop, NULL);

    // Simula recebimento de vetores de vizinhos (exemplo)
    int dv_from_1[MAX_NODES] = {0, 0, 2, 5, INF}; // vetor do vizinho 1
    int dv_from_2[MAX_NODES] = {0, 2, 0, INF, 7}; // vetor do vizinho 2

    // registra recebimentos e recomputa tabela
    receiveDistanceVector(1, dv_from_1);
    receiveDistanceVector(2, dv_from_2);
    recomputeRoutingTable();

    // menu simples
    int opc = 0;
    while(1){
        pthread_mutex_lock(&io_lock);
        printf("\nMenu:\n1 - Mostrar meu vetor-distancia\n2 - Mostrar vetores recebidos dos vizinhos\n3 - Mostrar tabela de roteamento\n4 - Recomputar tabela\n5 - Sair\nEscolha: ");
        fflush(stdout);
        if(scanf("%d", &opc) != 1) {
            pthread_mutex_unlock(&io_lock);
            break;
        }
        pthread_mutex_unlock(&io_lock);

        if(opc == 1) printMyDistanceVector();
        else if(opc == 2) printReceivedVectors();
        else if(opc == 3) printRoutingTable();
        else if(opc == 4) { recomputeRoutingTable(); pthread_mutex_lock(&io_lock); printf("Tabela recomputada.\n"); fflush(stdout); pthread_mutex_unlock(&io_lock); }
        else if(opc == 5) break;
    }

    // terminar thread
    sender_running = 0;
    pthread_join(sender_thread, NULL);

    return 0;
}
