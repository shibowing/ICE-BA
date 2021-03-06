/******************************************************************************
 * Copyright 2017-2018 Baidu Robotic Vision Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
#ifndef _ALIGNED_MATRIX_H_
#define _ALIGNED_MATRIX_H_

#include "SIMD.h"

template<class TYPE, int N>
class AlignedMatrixN {

 public:

  inline AlignedMatrixN() {
    m_own = true;
    m_rows[0] = NULL;
  }
  inline ~AlignedMatrixN() {
    if (m_rows[0] && m_own) {
      SIMD::Free<TYPE>(m_rows[0]);
    }
  }

  inline void Create() {
    if (m_rows[0] && m_own) {
      return;
    }
    m_own = true;
    m_rows[0] = SIMD::Malloc<TYPE>(N);
    for (int i = 1; i < N; ++i) {
      m_rows[i] = m_rows[i - 1] + N;
    }
  }
  inline void Destroy() {
    if (m_rows[0] && m_own) {
      SIMD::Free<TYPE>(m_rows[0]);
    }
    m_own = true;
    //m_rows[0] = NULL;
    memset(m_rows, 0, sizeof(m_rows));
  }

  inline void Set(TYPE *data) {
    Create(); memcpy(m_rows[0], data, sizeof(TYPE) * N * N);
  }
  inline void Bind(TYPE *data) {
    m_own = false;
    m_rows[0] = data;
    for (int i = 1; i < N; ++i) {
      m_rows[i] = m_rows[i - 1] + N;
    }
  }

  inline const TYPE* operator[] (const int i) const { return m_rows[i]; }
  inline       TYPE* operator[] (const int i)       { return m_rows[i]; }

 protected:

  bool m_own;
  TYPE *m_rows[N];
};

template<class TYPE>
class AlignedMatrixX {

 public:

  inline AlignedMatrixX() {
    m_own = true;
    m_symmetric = false;
    m_data = NULL;
    m_Nr = m_Nc = m_NcC = m_capacity = 0;
  }
  inline AlignedMatrixX(AlignedMatrixX &M) { *this = M; }
  inline ~AlignedMatrixX() {
    if (m_data && m_own) {
      SIMD::Free<TYPE>(m_data);
    }
  }

  inline void operator = (const AlignedMatrixX &M) {
    //Bind(M.Data(), M.GetRows(), M.GetColumns(), M.Symmetric());
    m_own = false;
    m_symmetric = M.m_symmetric;
    m_data = M.m_data;
    m_Nr = M.m_Nr;
    m_Nc = M.m_Nc;
    m_NcC = M.m_NcC;
    m_capacity = M.m_capacity;
    m_rows = M.m_rows;
  }
  inline const TYPE* operator[] (const int i) const { return m_rows[i]; }
  inline       TYPE* operator[] (const int i)       { return m_rows[i]; }

  inline const TYPE* Data() const { return m_data; }
  inline       TYPE* Data()       { return m_data; }
  inline const TYPE** RowsData() const { return m_rows.data(); }
  inline       TYPE** RowsData()       { return m_rows.data(); }

  inline void Resize(const int Nr, const int Nc, const bool retain = false,
                     const bool symmetric = false) {
    if (Nr == m_Nr && Nc == m_Nc && symmetric == m_symmetric) {
      return;
    }
#ifdef CFG_DEBUG
    if (symmetric) {
      UT_ASSERT(Nr == Nc);
      UT_ASSERT(SIMD::Ceil<TYPE>(Nc) == Nc);
    }
#endif
    const int NcC = symmetric ? Nc : SIMD::Ceil<TYPE>(Nc);
    const int N = symmetric ? ((Nc * (Nc + 1)) >> 1) : Nr * NcC;
    if (N > m_capacity) {
      TYPE *dataBkp = m_data;
      m_data = SIMD::Malloc<TYPE>(N);
      if (dataBkp) {
        if (retain) {
          //memcpy(m_data, dataBkp, sizeof(TYPE) * N);
#ifdef CFG_DEBUG
          UT_ASSERT(m_symmetric == symmetric);
#endif
          TYPE *row = m_data;
          if (symmetric) {
            size_t size = sizeof(TYPE) * m_Nc;
            for (int i = 0; i < m_Nr; ++i, size -= sizeof(TYPE)) {
              memcpy(row, m_rows[i] + i, size);
              row += NcC - i;
            }
          } else {
            const size_t size = sizeof(TYPE) * m_Nc;
            for (int i = 0; i < m_Nr; ++i) {
              memcpy(row, m_rows[i], size);
              row += NcC;
            }
          }
        }
        if (m_own) {
          SIMD::Free<TYPE>(dataBkp);
        }
      }
      m_own = true;
      m_NcC = NcC;
      m_capacity = N;
    } else if (Nr < m_Nr && Nc < m_NcC) {
#ifdef CFG_DEBUG
      UT_ASSERT(m_symmetric == symmetric);
#endif
      m_Nr = Nr;
      m_Nc = Nc;
      m_rows.resize(Nr);
      return;
    }
    m_symmetric = symmetric;
    m_Nr = Nr;
    m_Nc = Nc;
    m_rows.resize(Nr);
    if (Nr > 0) {
      m_rows[0] = m_data;
      if (symmetric) {
        for (int i = 1; i < m_Nr; ++i) {
          m_rows[i] = m_rows[i - 1] + m_NcC - i;
        }
      } else {
        for (int i = 1; i < m_Nr; ++i) {
          m_rows[i] = m_rows[i - 1] + m_NcC;
        }
      }
    }
  }

  inline void Copy(const AlignedMatrixX<TYPE> &M) {
#ifdef CFG_DEBUG
    UT_ASSERT(m_symmetric == M.m_symmetric);
    UT_ASSERT(m_Nr == M.m_Nr && m_Nc == M.m_Nc);
#endif
    if (m_symmetric) {
      size_t size = sizeof(TYPE) * m_Nc;
      for (int i = 0; i < m_Nr; ++i, size -= sizeof(TYPE)) {
        memcpy(m_rows[i] + i, M[i] + i, size);
      }
    } else {
      const size_t size = sizeof(TYPE) * m_Nc;
      for (int i = 0; i < m_Nr; ++i) {
        memcpy(m_rows[i], M[i], size);
      }
    }
  }

  inline void Bind(void *M, const int Nr, const int Nc, const bool symmetric = false) {
    if (m_data && m_own) {
      SIMD::Free<TYPE>(m_data);
    }
    m_data = (TYPE *) M;
#ifdef CFG_DEBUG
    if (symmetric) {
      UT_ASSERT(Nr == Nc);
      UT_ASSERT(SIMD::Ceil<TYPE>(Nc) == Nc);
    }
#endif
    const int NcC = symmetric ? Nc : SIMD::Ceil<TYPE>(Nc);
    const int N = symmetric ? ((Nc * (Nc + 1)) >> 1) : Nr * NcC;
    m_symmetric = symmetric;
    m_Nr = Nr;
    m_Nc = Nc;
    m_NcC = NcC;
    m_capacity = N;
    m_own = false;
    m_rows.resize(Nr);
    if (Nr > 0) {
      m_rows[0] = m_data;
      if (symmetric) {
        for (int i = 1; i < m_Nr; ++i) {
          m_rows[i] = m_rows[i - 1] + NcC - i;
        }
      } else {
        for (int i = 1; i < m_Nr; ++i) {
          m_rows[i] = m_rows[i - 1] + NcC;
        }
      }
    }
  }
  inline void* BindNext() { return m_data ? (m_data + m_capacity) : NULL; }
  inline int BindSize(const int Nr, const int Nc, const bool symmetric = false) const {
#ifdef CFG_DEBUG
    if (symmetric) {
      UT_ASSERT(Nr == Nc);
      UT_ASSERT(SIMD::Ceil<TYPE>(Nc) == Nc);
    }
#endif
    return sizeof(TYPE) * (symmetric ? ((Nc * (Nc + 1)) >> 1) : Nr * SIMD::Ceil<TYPE>(Nc));
  }

  inline void Swap(AlignedMatrixX<TYPE> &M) {
    UT_SWAP(m_own, M.m_own);
    UT_SWAP(m_symmetric, M.m_symmetric);
    UT_SWAP(m_data, M.m_data);
    UT_SWAP(m_Nr, M.m_Nr);
    UT_SWAP(m_Nc, M.m_Nc);
    UT_SWAP(m_NcC, M.m_NcC);
    UT_SWAP(m_capacity, M.m_capacity);
    m_rows.swap(M.m_rows);
  }

  inline void Set(const AlignedMatrixX<TYPE> &M) {
    Resize(M.GetRows(), M.GetColumns(), false, M.Symmetric());
    Copy(M);
  }

  inline AlignedMatrixX<TYPE> GetColumn(const int j, const int Nr) {
#ifdef CFG_DEBUG
    UT_ASSERT(j >= 0 && j < m_Nc);
#endif
    AlignedMatrixX<TYPE> M;
    M.m_own = M.m_symmetric = false;
    M.m_data = m_data + j;
    M.m_Nr = Nr;
    M.m_Nc = 1;
    M.m_NcC = m_NcC - j;
    M.m_capacity = Nr * M.m_NcC;
    M.m_rows.resize(Nr);
    for (int i = 0; i < Nr; ++i) {
      M.m_rows[i] = &m_rows[i][j];
    }
    return M;
  }
  inline AlignedMatrixX<TYPE> GetBlock(const int Nr, const int Nc) {
#ifdef CFG_DEBUG
    UT_ASSERT(Nr >= 0 && Nr <= m_Nr);
    UT_ASSERT(Nc >= 0 && Nc <= m_Nc);
#endif
    AlignedMatrixX<TYPE> M;
    M.m_own = false;
    M.m_symmetric = m_symmetric;
    M.m_data = m_data;
    M.m_Nr = Nr;
    M.m_Nc = Nc;
    M.m_NcC = m_NcC;
    M.m_capacity = Nr * M.m_NcC;
    M.m_rows.resize(Nr);
    for (int i = 0; i < Nr; ++i) {
      M.m_rows[i] = m_rows[i];
    }
    return M;
  }
  inline AlignedMatrixX<TYPE> GetBlock(const int i, const int j, const int Nr, const int Nc) {
#ifdef CFG_DEBUG
    UT_ASSERT(i >= 0 && i + Nr <= m_Nr);
    UT_ASSERT(j >= 0 && j + Nc <= m_Nc);
#endif
    AlignedMatrixX<TYPE> M;
    M.m_own = false;
    M.m_symmetric = m_symmetric;
    M.m_data = m_rows[i] + j;
    M.m_Nr = Nr;
    M.m_Nc = Nc;
    M.m_NcC = m_NcC - j;
    M.m_capacity = Nr * M.m_NcC;
    M.m_rows.resize(Nr);
    for (int _i = 0; _i < Nr; ++_i) {
      M.m_rows[_i] = M.m_rows[i + _i] + j;
    }
    return M;
  }

  inline void Erase(const int i) {
    if (m_symmetric) {
      for (int j = 0; j <= i; ++j) {
        TYPE *row = m_rows[j];
        for (int k1 = i, k2 = k1 + 1; k2 < m_Nc; k1 = k2++) {
          row[k1] = row[k2];
        }
      }
      size_t size = sizeof(TYPE) * (m_Nc - i - 1);
      for (int i1 = i, i2 = i1 + 1; i2 < m_Nr; i1 = i2++, size -= sizeof(TYPE)) {
        memcpy(m_rows[i1] + i1, m_rows[i2] + i2, size);
      }
    } else {
      for (int j = 0; j < m_Nr; ++j) {
        TYPE *row = m_rows[j];
        for (int k1 = i, k2 = k1 + 1; k2 < m_Nc; k1 = k2++) {
          row[k1] = row[k2];
        }
      }
      const size_t size = sizeof(TYPE) * (m_Nc - 1);
      for (int i1 = i, i2 = i1 + 1; i2 < m_Nr; i1 = i2++) {
        memcpy(m_rows[i1], m_rows[i2], size);
      }
    }
    Resize(m_Nr - 1, m_Nc - 1, true, m_symmetric);
  }

  inline void InsertZero(const int i, AlignedVector<float> *work) {
//#ifdef CFG_DEBUG
#if 0
    UT_ASSERT(i >= 0 && i <= m_Nr && i <= m_Nc);
#endif
    if (i < m_Nr && i < m_Nc) {
      AlignedMatrixX<TYPE> MTmp;
      work->Resize(MTmp.BindSize(m_Nr, m_Nc, m_symmetric) / sizeof(float));
      MTmp.Bind(work->Data(), m_Nr, m_Nc, m_symmetric);
      MTmp.Set(*this);
      Resize(m_Nr + 1, m_Nc + 1, false, m_symmetric);
      if (m_symmetric) {
        size_t size1 = sizeof(TYPE) * i, size2 = sizeof(TYPE) * (m_Nc - 1 - i);
        for (int j = 0; j < i; ++j, size1 -= sizeof(TYPE)) {
          memcpy(m_rows[j] + j, MTmp[j] + j, size1);
          memset(m_rows[j] + i, 0, sizeof(TYPE));
          memcpy(m_rows[j] + i + 1, MTmp[j] + i, size2);
        }
        memset(m_rows[i] + i, 0, sizeof(TYPE) * (m_Nc - i));
        for (int i1 = i, i2 = i + 1; i2 < m_Nr; i1 = i2++, size2 -= sizeof(TYPE)) {
          memcpy(m_rows[i2] + i2, MTmp[i1] + i1, size2);
        }
      } else {
        const size_t size1 = sizeof(TYPE) * i, size2 = sizeof(TYPE) * (m_Nc - 1 - i);
        for (int j = 0; j < i; ++j) {
          memcpy(m_rows[j], MTmp[j], size1);
          memset(m_rows[j] + i, 0, sizeof(TYPE));
          memcpy(m_rows[j] + i + 1, MTmp[j] + i, size2);
        }
        memset(m_rows[i], 0, sizeof(TYPE) * m_Nc);
        for (int i1 = i, i2 = i + 1; i2 < m_Nr; i1 = i2++) {
          memcpy(m_rows[i2], MTmp[i1], size1);
          memset(m_rows[i2] + i, 0, sizeof(TYPE));
          memcpy(m_rows[i2] + i + 1, MTmp[i1] + i, size2);
        }
      }
    } else {
      const int Nr = m_Nr, Nc = m_Nc;
      Resize(Nr + 1, Nc + 1, true, m_symmetric);
      for (int i = 0; i < m_Nr; ++i) {
        memset(&m_rows[i][Nc], 0, sizeof(TYPE));
      }
      if (!m_symmetric) {
        memset(m_rows[Nr], 0, sizeof(TYPE) * Nc);
      }
    }
  }

  inline void MakeZero() {
    if (m_symmetric) {
      size_t size = sizeof(TYPE) * m_Nc;
      for (int i = 0; i < m_Nr; ++i, size -= sizeof(TYPE)) {
        memset(m_rows[i] + i, 0, size);
      }
    } else {
      const size_t size = sizeof(TYPE) * m_Nc;
      for (int i = 0; i < m_Nr; ++i) {
        memset(m_rows[i], 0, size);
      }
    }
  }

  inline bool Symmetric() const { return m_symmetric; }

  inline int GetRows() const { return m_Nr; }
  inline int GetColumns() const { return m_Nc; }
  inline int GetRowStride() const { return m_NcC; }
  inline void GetRow(const int i, TYPE *r) const { memcpy(r, m_rows[i], sizeof(TYPE) * m_Nc); }

  inline void SaveB(FILE *fp) const {
    UT::SaveB<int>(m_Nr, fp);
    UT::SaveB<int>(m_Nc, fp);
    UT::SaveB<bool>(m_symmetric, fp);
    if (m_symmetric) {
      for (int i = 0; i < m_Nr; ++i) {
        UT::SaveB(m_rows[i] + i, m_Nc - i, fp);
      }
    } else {
      for (int i = 0; i < m_Nr; ++i) {
        UT::SaveB(m_rows[i], m_Nc, fp);
      }
    }
  }
  inline void LoadB(FILE *fp) {
    const int Nr = UT::LoadB<int>(fp);
    const int Nc = UT::LoadB<int>(fp);
    const bool symmetric = UT::LoadB<bool>(fp);
    Resize(Nr, Nc, false, symmetric);
    if (m_symmetric) {
      for (int i = 0; i < m_Nr; ++i) {
        UT::LoadB(m_rows[i] + i, m_Nc - i, fp);
      }
    } else {
      for (int i = 0; i < m_Nr; ++i) {
        UT::LoadB(m_rows[i], m_Nc, fp);
      }
    }
  }
  inline bool SaveB(const std::string fileName) const {
    FILE *fp = fopen(fileName.c_str(), "wb");
    if (!fp) {
      return false;
    }
    SaveB(fp);
    fclose(fp);
    UT::PrintSaved(fileName);
    return true;
  }
  inline bool LoadB(const std::string fileName) {
    FILE *fp = fopen(fileName.c_str(), "rb");
    if (!fp) {
      return false;
    }
    LoadB(fp);
    fclose(fp);
    UT::PrintLoaded(fileName);
    return true;
  }

  inline float MemoryMB() const { return m_own ? 0 : UT::MemoryMB<TYPE>(m_capacity); }

 protected:

   bool m_own, m_symmetric;
   TYPE *m_data;
   int m_Nr, m_Nc, m_NcC, m_capacity;
   std::vector<TYPE *> m_rows;
};

#endif
