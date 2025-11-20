# Sistema Inteligente de Controle de Estoque com ESP32, RFID e Integração a API REST

Projeto final desenvolvido para a disciplina de Database of Things do curso de Engenharia de Computação (UPF). O sistema integra hardware embarcado (ESP32), comunicação web (API REST) e banco de dados orientado a documentos (MongoDB) para gerenciar entradas e saídas de um almoxarifado físico.

## TECNOLOGIAS UTILIZADAS:
 
-> **Linguagem:** Python 3.10+  
-> **Framework Web:** FastAPI  
-> **Servidor:** Uvicorn  
-> **Banco de Dados:** MongoDB (Driver: PyMongo)  
-> **Validação de Dados:** Pydantic  

## Funcionalidades da API

A API serve como *middleware* entre o hardware e o banco de dados, possuindo as seguintes responsabilidades:

1.  **Gerenciamento de Itens:** CRUD completo (Criar, Ler, Atualizar, Deletar) para os itens do estoque.  
2.  **Registro de Movimentação:** Recebe sinais do ESP32 (Entrada/Saída) via RFID, atualiza o saldo do estoque e gera um log histórico.  
3.  **Dashboard Data:** Fornece endpoints para alimentar o front-end de visualização.  

## INSTALAÇÃO E EXECUÇÃO

Siga os passos abaixo para rodar a API localmente.

1. Clonar o repositório
```
git clone [https://github.com/zNyctus/DataBaseOfThings.git](https://github.com/zNyctus/DataBaseOfThings.git)
cd DataBaseOfThings
```
2. Criar e ativar ambiente virtual
```
python -m venv .venv
```
Para o Windows, se necessário: 
```
Set-ExecutionPolicy Unrestricted -Scope Process 
```
```
.\.venv\Scripts\activate
```
**ou**
```
source venv\Scripts\activate
```
3. Instalar dependências
```
pip install -r requirements.txt
```
**4. Se não existir, criar arquivo de nome ".env" no mesmo diretório, e colar o seguinte texto:**

```env
MONGO_URL=mongodb://197402:197402@177.67.253.61:27017/?authSource=197402
```

5. Executar o servidor
```
uvicorn api:app --reload
```

## DOCUMENTAÇÃO DOS ENDOPOINTS 

- **POST /movimentacoes** → (ESP32) Registra entrada/saída via RFID.  
- **GET /itens** → (Front-end) Lista estoque atual.  
- **POST /itens** → Cadastra novo item (RFID ↔ Nome).  
- **PUT /itens/{uid}** → Atualiza nome do item.  
- **DELETE /itens/{uid}** → Remove item do sistema.  

## ESTRUTURA DO BANCO DE DADOS (MongoDB)

- **Collection itens:** Armazena o estado atual (_id = UID do RFID).  
- **Collection movimentacoes:** Armazena o histórico (Logs de todas as operações).  

AUTORES

**Brenda Slongo Taca - 197402**  
**Felipe Borges - 184387**  
