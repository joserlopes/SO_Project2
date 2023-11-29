# Projeto Sistemas Operativos - Exercício 2

Projeto realizado por:

- João Fidalgo - Nº103471
- José Lopes - Nº103938

----

## Manual de utilização

- Quando nos comandos dos clientes se faz referência a um nome de uma box, é suposto esse nome aparecer sem o caracter '/' inicial. O código já trata disso antes de manipular o respetivo ficheiro.

## Observações

- Grande parte das funcionalidades ficaram por implementar, nomeadamente:
    - _Producer-consumer_ queue (a mais importante de todas);
    - Sincronização entre a escrita/leitura das boxes por parte dos publishers/subscribers;
    - Sincronização entre as diferentes threads.

- No entanto, o esqueleto do servidor ficou relativamente completo. O mbroker lança as threads correspondentes ao número recebido como argumento da linha de comandos, que depois são postas a correr em ciclo com os handlers dos vários pedidos e respetivas respostas, estes também relativamente implementados.
