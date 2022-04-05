from lorawan_rpi import YuboxLora
from random import randint
from time import sleep

YuboxLora = YuboxLora(deveui = "feedc0decafef00d",
						appkey = "2a3c5295f053a148e433e872c7104e1c")

while True:
	try:
		texto = "{}-{}".format(randint(1,2500)/100,randint(1,2500)/100) 
		YuboxLora.SendData(texto)
		print("Datos Enviados Exitosamente")
		sleep(3)

	except KeyboardInterrupt:
		YuboxLora.Close()
		break
