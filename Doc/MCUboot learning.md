# MCU boot flash layout

* Primary Slot (Slot 0)：当前 App 运行区。MCUboot 永远只从这个区域引导启动。
* Secondary Slot (Slot 1)：新固件下载暂存区。
* Scratch Area（暂存区）：用于在两个 Slot 之间交换（Swap）数据时作为中转站（如果不使用直接擦写模式的话）。


# MCU boot image trailer
## 标志位（flag）以及作用描述
* Magic: 一个特定的固定值，表示该trailer数据有效
* Swap status: 记录交换进度，如果升级过程中断电，MCUboot靠它恢复断点
* Copy done: 标志secondary slot的数据是否已经完整复制到了primary slot
* Image ok: 最关键的防砖标志，表示当前运行的固件已经被App自身确认为健康可用

## MCUboot标准升级执行流程（Swap模式）
  1. App下载并触发升级
  正在运行的旧APP接收到新固件，将其写入secondary slot。下载完成后，APP会调用接口，在secondary slot的trailer中写入magic。此时，固件处于pending状态，随后系统软复位
  1. MCUboot启动与校验
  复位后MCUboot接管，它读取两个slot的trailer。发现secondary slot有合法的magic且标记为待升级，MCUboot会首先对secondary slot中的新固件进行哈希（SHA-256）和签名（RSA/ECDSA）校验，如果校验失败，直接擦除secondary slot并启动原APP；如果成功，进入交换阶段
  1. 扇区交换（Swap via scratch）
  MCUboot开始将primary slot和secondary slot的内容进行交换，为了方式断点丢失数据，它每次只交换一个扇区：
      a. 将primary 的一个扇区拷入scratch
      b. 将secondary 对应的扇区拷入primary
      c. 将scratch中的数据拷入secondary（每次操作都会更新swap status标志位，如果此时断电，MCUboot再次上电时会读取该标志位，继续未完成的搬运。）
  1. 交换完成与启动新App
  所有扇区交换完毕后，MCUboot在primary slot的trailer中写入copy done标志，此时，新固件在primary slot，旧固件被备份到了secondary slot。MCUboot执行环境清理，将PC指针跳转到primary slot，启动新APP
  1. App确认固件与回滚机制
  新APP启动前，必须在有限时间内完成自检，并调用boot_set_confirmed（）接口，这会在primary slot的trailer中写入image ok标志，如果APP存在致命bug导致死机，看门狗复位，或者未能写入image ok标志，MCU重启后，MCUboot发现primary slot没有image ok，就会判定升级失败，触发回滚机制，再次执行step3的swap过程，将旧固件换回primary slot并启动
    
其它升级策略
    1. Overwrite only(仅覆盖)：最简单粗暴。校验新固件后，直接把 Secondary Slot 拷贝覆盖到 Primary Slot。不需要 Scratch 区，速度快，但不支持回滚（旧固件直接被抹除了）。
    2. Direct XIP(直接片上执行)：不需要把固件搬运到 Primary Slot。MCUboot 校验后，通过修改 MCU 的地址映射或者 VTOR，直接让 MCU 从 Secondary Slot 运行新固件。这极大节省了 Flash 寿命和启动时间，但要求 MCU 的 Flash 支持灵活的代码执行和向量表重定向。

## MCUboot固件加密手段以及方法
MCUboot处理加密固件的核心思想是混合加密（hybrid encryption）。不会采用同一个静态密钥来加密所有固件，而是将对称加密（AES）和非对称加密（RSA/ECIES）结合起来。

两个关键密钥：
    1. CEK（content encryption key）：一个随机生成的AES密钥（通常是AES-CTR-128或AES-CTR-256），专门用来加密固件本体。每次打包固件时，这个密钥都是全新生成的
    2. KEK（key encryption key）：一对非对称密钥（RSA/ECDSA），公钥放在服务器端的打包工具里，私钥安全地烧录在MCU的内部flash或安全模块中



密钥分配与解密：
 1. 固件打包与加密（云端/PC端）
 当使用MCUboot的imgtool脚本打包新固件时，工具在后台执行了一下操作
        a. 生成CEK：imgtool在电脑上随机生成一个16字节或32字节的AES密钥
        b. 加密固件：使用这个随机的CEK，以AES-CTR模式对APP固件本体进行加密
        c. 加密CEK：使用提前准备好的非堆成密钥公钥（public KEK），将刚才生成的随机CEK加密
        d. 组装镜像：将加密后的固件主题，数字签名，以及被加密过的CEK，一起打包成一个带有TLV（type-length-value）尾部的标准MCUboot镜像文件
 1. OTA传输与flash存储
        a. OTA传输：上位机或云端将打包好的加密镜像发送给MCU。此时在空中传输的完全时密文，即使被抓包，黑客也无法逆向工程
        b. 存入secondary slot：MCU运行的旧APP接收到数据后，原封不动的将其写入外部flash或内部的secondary slot。此时，暂存区里存放的依然时密文固件和加密的CKE。
 2. bootloader解密与升级
        a. 提取加密的CEK：MCUboot解析secondary slot镜像的TLV区域，找到被加密的AES密钥（CEK）
        b. 非对称解密获取CEK：MCUboot使用烧录在设备内部的安全私钥（private KEK），通过RSA或ECIES算法解密这个TLV数据，还原出明文的AEK密钥（CEK）
        c. 边搬运边解密（swap & decrypt）：
            i. 在执行经典的swap操作时，MCUboot从secondary slot读取一块密文数据
            ii. 使用刚才还原出的AES密钥（CEK），通过AES-CTR模式在RAM中将这块数据解密成明文
            iii. 将解密后的明文数据写入primary slot
 3. 启动新固件：当整个swap过程完成后，primary slot中存放的已经是完全解密好的明文APP代码。MCUboot完成清理工作后，直接跳转primary slot执行。

