from fastapi import FastAPI
from pydantic import BaseModel
import pymongo 


#Se for preciso essa chatice aqui Set-ExecutionPolicy Unrestricted -Scope Process
#.\.venv\Scripts\activate
#pip install -r requirements.txt
#felicidade

client = pymongo.MongoClient("SUA_CONNECTION_STRING_MONGO")
db = client["database_of_things"]
col_itens = db["itens"]
col_movimentacoes = db["movimentacoes"]

app = FastAPI()

class Movimentacao(BaseModel):
    rfid_uid: str
    acao: str
    quantidade: int

@app.post("/movimentacoes")
async def registrar_movimentacao(mov: Movimentacao):
    
    item = col_itens.find_one({"_id": mov.rfid_uid})
    
    if not item:
        return {"message": "Erro: Item não cadastrado", "estoque_atual": 0}

    nova_quantidade = item["quantidade"]
    if mov.acao == "entrada":
        nova_quantidade += mov.quantidade
    elif mov.acao == "saida":
        nova_quantidade -= mov.quantidade

    col_itens.update_one(
        {"_id": mov.rfid_uid},
        {"$set": {"quantidade": nova_quantidade}}
    )
    
    #Registrar o log da movimentação
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