def cpu_status(args):
    machine = self.Machine
    
    # Find the CPU peripheral
    cpu = None
    for peripheral in machine.SystemBus.GetCPUs():
        cpu = peripheral
        break
    
    if cpu is None:
        print("CPU not found!")
        return
    
    print("="*50)
    print("CPU STATUS REPORT")
    print("="*50)
    print("Halted:              {}".format(cpu.IsHalted))
    print("Program Counter:     {}".format(cpu.PC))
    print("Stack Pointer:       {}".format(cpu.SP))
    print("Instructions Exec:   {}".format(cpu.ExecutedInstructions))
    print("LR (Return):         {}".format(cpu.LR))
    print("="*50)