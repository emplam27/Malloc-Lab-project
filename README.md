# Malloc Lab porject

C 언어의 `malloc`과 `free`를 구현하는 프로젝트 입니다. 블로그 포스팅으로 동적할당, 프로젝트 회고록을 작성하였습니다.

[[CS] 그림으로 알아보는 메모리 동적할당 - Implicit, Explicit, Segregated list Allocator](https://velog.io/@emplam27/CS-%EA%B7%B8%EB%A6%BC%EC%9C%BC%EB%A1%9C-%EC%95%8C%EC%95%84%EB%B3%B4%EB%8A%94-%EB%A9%94%EB%AA%A8%EB%A6%AC-%EB%8F%99%EC%A0%81%ED%95%A0%EB%8B%B9-Implicit-Explicit-Segregated-list-Allocator)

[[회고록] Malloc Lab 프로젝트](https://velog.io/@emplam27/%ED%9A%8C%EA%B3%A0%EB%A1%9D-Malloc-Lab-%ED%94%84%EB%A1%9C%EC%A0%9D%ED%8A%B8)



### 코드 실행 방법

```bash
# ubuntu 환경
sudo apt update
sudo apt install build-essential
sudo apt install gdb
sudo apt-get install gcc-multilib g++-multilib

git clone https://github.com/emplam27/Malloc-Lab-project.git
cd Malloc-Lab-project/malloc-mdriver

# mm.c 파일내용을 explicit-malloc.c, implicit-malloc(first-fit).c, implicit-malloc(next-fit).c로 수정
make re
```

### 