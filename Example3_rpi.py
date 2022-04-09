from lorawan.lorawan_rpi import YuboxLora
from random import randint
from time import sleep

#Funciones Callback
def OnMessageLora(ssr: int ,msg: str):
    #Cuando nos llega la respuesta del servidor procedemos a utilizarla
    list_data = msg.split("-")  

    #Resultado del procesamiento
    result = list_data[1]

    #Segun el resultado realizamos una accion
    if (result == "ON"):
        accion = "Cerrar toldo"
    else:
        accion = "Abrir toldo"

    print(f"LLego mensaje: {msg}")
    print(f"Se procede a {accion}\n\n")


#Creamos el objeto YuboxLora y le pasamos nuestras credenciales
YuboxLora = YuboxLora(deveui = "deveui",
						appkey = "appkey",
						callbackMessage= OnMessageLora)


#Iniciamos el bucle
while True:
	try:
        #Simulamos un sensor
		temp = randint(2500,3200)/100
		hum  = randint(10,1000)/10

		#Generamos el texto
		texto = f"{temp}-{hum}"
        
		#Pasamos el texto
		YuboxLora.SendData(texto)
        
		#Esperamos un poco para no saturar
		print(f"Datos Enviados:\nTemperatura: {temp} Humedad: {hum}")
		sleep(30)

	except KeyboardInterrupt:
		#Para que no se nos bloque los pines GPIO, debemos cerrar
		YuboxLora.Close()
		break
