import paho.mqtt.client as mqtt
import json
import base64

def on_connect(client, userdata, flags, rc) -> None:
    print("Conectado")
    client.subscribe(f"application/5/device/{userdata['deveui']}/event/up")


def on_disconnet(client, userdata, rc) -> None:
    print("Desconectado")
    client.reconnect_delay_set(min_delay=1, max_delay=120)


def on_message(client, userdata, message) -> None:
    mensaje = json.loads(str(message.payload.decode("utf-8")).strip())
    topico = str(message.topic).strip()

    #Separamos los datos del mensaje
    dataBase64 = mensaje["data"]
    rxInfo = mensaje["rxInfo"][0]
    name_gatewey = rxInfo["name"]

    #Si es none saltamos todo el proceso
    if dataBase64 == None:
        return

    # El mensaje se encuentra en Base64, lo pasamos a string
    base64_bytes = dataBase64.encode('ascii')
    message_bytes = base64.b64decode(base64_bytes)
    message = message_bytes.decode('ascii')

    #El mensaje debe tener el siguiente formato
    #temperatura-humedad

    list_message = message.split("-")
    temp = float(list_message[0])
    hum = float(list_message[1])

    #Realizamos el procesamiento correspondiente
    if (temp>=30 and hum>75):
        respuesta = "LLover-ON"
    else:
        respuesta = "Llover-OFF"

    #Debemos pasarle el mensaje en Base64
    message_bytes = respuesta.encode('ascii')
    base64_bytes = base64.b64encode(message_bytes)
    base64_message = base64_bytes.decode('ascii')

    #El comando necesita un json
    json_data = {"confirmed":False, "fPort" : 2, "data" : base64_message}

    #Lo convertimos en texto
    msg_json =  json.dumps(json_data)

    topic = f"application/5/device/{userdata['deveui']}/command/down"
    client.publish(topic, msg_json)

    #Mostrar Datos por pantalla
    print("Datos recibidos:")
    print(f"Temperatura: {temp} Humedad: {hum}")
    print("Procesamiento de datos:")
    print(f"{respuesta}")
    print("")


#----------------------------------------------
#Importante Cambiar
deveui = "deveui"
#----------------------------------------------

#Credenciales Servidor MQTT para Iotodos Red Lora
user = ""
password = ""
host = "zensor.ec"
port = 1883

#Creeamos los client
client = mqtt.Client(userdata={"deveui":deveui})
client.username_pw_set(username=user, password=password)

#Asignamos las funciones callback
client.on_connect = on_connect
client.on_disconnect = on_disconnet
client.on_message = on_message

#Nos conectamos
try:
    client.connect(host, port, 60)
except OSError:
    client.reconnect_delay_set(min_delay=1, max_delay=120)

#Empezamos el bucle
try:
    client.loop_forever()
except KeyboardInterrupt:
    print("Proceso Finalizado")