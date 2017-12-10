addi 1,0,14
addi 2,0,-1
slt 3,2,1
addi 1,1,-1
ble 1,0, done
addi 4,0,1
slt 4,1,2
xor 5,1,2
jr 3
done:
addi 9,0,1111