resultado  1


PS C:\Users\burge\Documents\quanti> python quanti_quant_experiment.py --model Qwen/Qwen2.5-0.5B --samples 4 --seeds 5
C:\Users\burge\AppData\Local\Programs\Python\Python312\python.exe: can't open file 'C:\\Users\\burge\\Documents\\quanti\\quanti_quant_experiment.py': [Errno 2] No such file or directory
PS C:\Users\burge\Documents\quanti> python main.py --model Qwen/Qwen2.5-0.5B --samples 4 --seeds 5                   
Cargando Qwen/Qwen2.5-0.5B en cpu...
Warning: You are sending unauthenticated requests to the HF Hub. Please set a HF_TOKEN to enable higher rate limits and faster downloads.
config.json: 100%|█████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████| 681/681 [00:00<?, ?B/s]
C:\Users\burge\AppData\Local\Programs\Python\Python312\Lib\site-packages\huggingface_hub\file_download.py:139: UserWarning: `huggingface_hub` cache-system uses symlinks by default to efficiently store duplicated files but your machine does not support them in C:\Users\burge\.cache\huggingface\hub\models--Qwen--Qwen2.5-0.5B. Caching files will still work but in a degraded version that might require more space on your disk. This warning can be disabled by setting the `HF_HUB_DISABLE_SYMLINKS_WARNING` environment variable. For more details, see https://huggingface.co/docs/huggingface_hub/how-to-cache#limitations.
To support symlinks on Windows, you either need to activate Developer Mode or to run Python as an administrator. In order to activate developer mode, see this article: https://docs.microsoft.com/en-us/windows/apps/get-started/enable-your-device-for-development
  warnings.warn(message)
tokenizer_config.json: 100%|███████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████| 7.23k/7.23k [00:00<?, ?B/s]
vocab.json: 100%|██████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████| 2.78M/2.78M [00:00<00:00, 9.71MB/s]
merges.txt: 100%|██████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████| 1.67M/1.67M [00:00<00:00, 15.8MB/s]
tokenizer.json: 100%|██████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████| 7.03M/7.03M [00:00<00:00, 95.2MB/s]
[transformers] `torch_dtype` is deprecated! Use `dtype` instead!
model.safetensors: downloading bytes: ██████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████|  855MB, 40.4kB/s  
model.safetensors: reconstructing file: 100%|██████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████|  988MB /  988MB, 49.1kB/s  
Loading weights: 100%|███████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████| 290/290 [00:00<00:00, 820.32it/s]
generation_config.json: 100%|██████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████| 138/138 [00:00<?, ?B/s]
Capas Linear encontradas (excluyendo lm_head): 168

[Baseline FP32]        perplexity = 19.3594
[RTN-INT4 / map]       perplexity = 46.6404   MSE_pesos_avg = 1.601141e-05
[Stochastic-INT4 / sample, 1 muestra x5 seeds]
                       perplexity media = 96.0800   std = 20.3429
                       MSE_pesos_avg = 3.231924e-05
                       (rango observado: 73.8528 - 133.1215)

Corriendo ensemble de K=4 muestras estocasticas (promedio de logits)...
[Stochastic-INT4 / ensemble K=4]  perplexity = 55.7045

======================================================================
RESUMEN
======================================================================
Metodo                             Perplexity     Costo relativo 
FP32 (sin cuantizar)               19.3594        1x (referencia)
RTN-INT4 (map, deterministico)     46.6404        1x             
Stochastic-INT4 (sample, 1x)       96.0800        1x             
Stochastic-INT4 (ensemble K=4)     55.7045        4x             

Lectura esperada (hipotesis a confirmar/refutar con estos numeros):
 - RTN deberia ganarle a Stochastic de 1 sola muestra (menos varianza).
 - Stochastic-ensemble deberia acercarse mas a FP32 que RTN,
   pero cuesta K veces mas computo. Si NO se acerca, la hipotesis
   de 'colapso tardio' no aporta nada sobre este modelo/capas y
   hay que decirlo con esos numeros, no forzar la conclusion.
PS C:\Users\burge\Documents\quanti> 




