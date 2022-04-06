from xxlimited import Str
from lorawan.lorawan_rpi import YuboxLora
from random import randint
from time import sleep


def OnMessageLora(ssr: int ,msg: str):
	print(f"Mensaje Recibido: {msg}, con un SSR: {ssr}")


YuboxLora = YuboxLora(deveui = "feedc0decafef00d",
						appkey = "2a3c5295f053a148e433e872c7104e1c",
						callbackMessage= OnMessageLora)

while True:
	try:
		texto = "{}-{}".format(randint(1,2500)/100,randint(1,2500)/100) 
		YuboxLora.SendData(texto)
		print("Datos Enviados Exitosamente")
		sleep(5)
		pass

	except KeyboardInterrupt:
		YuboxLora.Close()
		break
