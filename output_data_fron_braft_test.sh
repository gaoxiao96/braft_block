#!/bin/bash

RES_txt='RES.txt'

for dest_txt  in 'braft1_.txt'
do
    res_txt=${dest_txt}

    cat ${res_txt} | grep RES | awk -F ' ' -v OFS='\t' '{print $2, $3, $4, $5, $6, $7}' >> ${RES_txt}
    
    echo >> ${RES_txt}
    echo >> ${RES_txt}

done