from fastapi import FastAPI, HTTPException, status
from pydantic import BaseModel
import pymongo 
from typing import List 
import os
from dotenv import load_dotenv
from fastapi.middleware.cors import CORSMiddleware

# Carrega as variáveis do arquivo .env
load_dotenv()

# Pega a URL do banco das variáveis de ambiente
mongo_url = os.getenv("MONGO_URL")

# Conecta ao Banco
# (Se der erro aqui, verifique se o arquivo .env está criado corretamente com a variável MONGO_URL)
client = pymongo.MongoClient(mongo_url)

# Teste de conexão
try:
    client.admin.command("ping")
    print("✅ Conexão com o MongoDB UPF bem-sucedida!")
except Exception as e:
    print("❌ Erro ao conectar ao MongoDB:", e)

db = client["197402"]
col_itens = db["itens"]
col_movimentacoes = db["movimentacoes"]


app = FastAPI()

origins = ["*"]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# --- MODELOS PYDANTIC ---

# Modelo para CADASTRAR um item novo (pelo PC)
class ItemCreate(BaseModel):
    rfid_uid: str  # "_id" no Mongo
    nome: str
    quantidade_inicial: int = 0

# Modelo para ATUALIZAR o nome (é em código) de um item 
class ItemUpdate(BaseModel):
    nome: str 

# Modelo para exibir itens (retorno da API)
class ItemInDB(BaseModel):
    rfid_uid: str
    nome: str
    quantidade: int

# Modelo para registrar uma movimentação 
class Movimentacao(BaseModel):
    rfid_uid: str
    acao: str      # "entrada" ou "saida"
    quantidade: int


# ======== Parte de itens ========

@app.post("/itens", status_code=status.HTTP_201_CREATED, response_model=ItemInDB)   
async def cadastrar_item(item: ItemCreate):
    """
    (PC) Cadastra um novo item (associa RFID a um nome).
    """
    # Verifica se o item já existe
    if col_itens.find_one({"_id": item.rfid_uid}):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, 
                            detail="Item com este RFID já cadastrado")
    
    # Prepara o documento para o MongoDB
    item_db = {
        "_id": item.rfid_uid,
        "nome": item.nome,
        "quantidade": item.quantidade_inicial
    }
    col_itens.insert_one(item_db)
    
    # Retorna o item criado no formato do ItemInDB
    return {"rfid_uid": item.rfid_uid, "nome": item.nome, "quantidade": item.quantidade_inicial}


@app.put("/itens/{rfid_uid}", response_model=ItemInDB)
async def atualizar_item(rfid_uid: str, item_update: ItemUpdate):
    """
    Atualiza o nome de um item existente.
    """
    result = col_itens.update_one(
        {"_id": rfid_uid},
        {"$set": {"nome": item_update.nome}}
    )
    
    if result.matched_count == 0:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, 
                            detail="Item não encontrado")
    
    item_atualizado = col_itens.find_one({"_id": rfid_uid})
    item_atualizado["rfid_uid"] = item_atualizado.pop("_id")
    return item_atualizado


@app.delete("/itens/{rfid_uid}")
async def deletar_item(rfid_uid: str):
    """
    Deleta um item do cadastro.
    """
    result = col_itens.delete_one({"_id": rfid_uid})
    
    if result.deleted_count == 0:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, 
                            detail="Item não encontrado")
    
    return {"message": "Item deletado com sucesso", "rfid_uid": rfid_uid}


@app.post("/movimentacoes")
async def registrar_movimentacao(mov: Movimentacao):
    
    # Quantidade deve ser positiva
    if mov.quantidade <= 0:
        raise HTTPException(
            status_code=400,
            detail="A quantidade deve ser maior que zero."
        )

    # Buscar item no banco
    item = col_itens.find_one({"_id": mov.rfid_uid})
    
    if not item:
        return {"message": "Erro: Item não cadastrado", "estoque_atual": 0}

    nova_quantidade = item["quantidade"]

    # Registrar ENTRADA
    if mov.acao == "entrada":
        nova_quantidade += mov.quantidade

    # Registrar SAÍDA (com validação de estoque)
    elif mov.acao == "saida":
        if item["quantidade"] - mov.quantidade < 0:
            raise HTTPException(
                status_code=400,
                detail=f"Estoque insuficiente. Estoque atual: {item['quantidade']}"
            )
        nova_quantidade -= mov.quantidade

    # Atualizar o item no DB
    col_itens.update_one(
        {"_id": mov.rfid_uid},
        {"$set": {"quantidade": nova_quantidade}}
    )

    # Registrar log
    col_movimentacoes.insert_one(mov.dict())
    
    return {"message": "Movimentação registrada", "estoque_atual": nova_quantidade}


# Endpoint que é pro frontend usar
@app.get("/itens")
async def listar_itens():
    # Busca todos os itens no banco
    todos_os_itens = list(col_itens.find({}, {"_id": 1, "nome": 1, "quantidade": 1}))
    
    # Tratamentozinho pra converter o id do mongo
    for item in todos_os_itens:
        item["rfid_uid"] = item.pop("_id")
        
    return todos_os_itens